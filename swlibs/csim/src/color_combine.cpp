#include <algorithm>

#include "color_combine.hpp"

namespace csim {

namespace {

/**
 * @brief Applies an invert function of combineMode.
 * @param[in] value The input of the function.
 * @param[in] select The invert bits of combineMode.
 * @param[in] zeroMinusX The value of @p select for 0 - x.
 * @param[in] oneMinusX The value of @p select for 1 - x.
 * @param[in] xMinusHalf The value of @p select for x - 0.5.
 */
int invert(int value, FxU32 select, FxU32 zeroMinusX, FxU32 oneMinusX, FxU32 xMinusHalf)
{
  if (select == zeroMinusX) return -value;
  if (select == oneMinusX) return 0xFF - value;
  if (select == xMinusHalf) return value - 0x80;
  return value;
}

/**
 * @brief Applies the output shift of combineMode.
 * @param[in] value The combined value.
 * @param[in] shift The shift bits of combineMode.
 * @param[in] twoX The value of @p shift for 2x.
 * @param[in] fourX The value of @p shift for 4x.
 */
int outShift(int value, FxU32 shift, FxU32 twoX, FxU32 fourX)
{
  if (shift == twoX) return value << 1;
  if (shift == fourX) return value << 2;
  return value;
}

}  // namespace

void combineColor(FxU8 local[4], const FxU8 other[4], const FxU8 mselect7[3],
                  const FxU8 texture[4], const CombineConfig &config)
{
  const FxU32 mode = config.mode;
  const FxU32 combineMode = config.combineMode;
  const int reverse = static_cast<int>(config.rgbReverse);
  int c[3];

  for (int i = 0; i < 3; i++) {
    int otherTemp = (mode & SST_TC_ZERO_OTHER) ? 0 : other[i];
    int localTemp = (mode & SST_TC_SUB_CLOCAL) ? local[i] : 0;
    otherTemp = invert(otherTemp, combineMode & SST_CM_TC_INVERT_OTHER,
                       SST_CM_TC_INVERT_OTHER_ZERO_MINUS_X, SST_CM_TC_INVERT_OTHER_ONE_MINUS_X,
                       SST_CM_TC_INVERT_OTHER_X_MINUS_HALF);
    localTemp = invert(localTemp, combineMode & SST_CM_TC_INVERT_LOCAL,
                       SST_CM_TC_INVERT_LOCAL_ZERO_MINUS_X, SST_CM_TC_INVERT_LOCAL_ONE_MINUS_X,
                       SST_CM_TC_INVERT_LOCAL_X_MINUS_HALF);
    c[i] = otherTemp + localTemp;
  }

  // The blend factor, 0..255 plus one, per channel or for all three.
  int beta = -1;
  const FxU8 *factors = nullptr;
  switch (mode & SST_TC_MSELECT) {
  case SST_TC_MONE:
  case SST_TC_MLODFRAC + SST_TC_MCLOCAL:
    beta = 0;
    break;
  case SST_TC_MCLOCAL: factors = local; break;
  case SST_TC_MAOTHER: beta = other[3]; break;
  case SST_TC_MALOCAL: beta = local[3]; break;
  case SST_TC_MLOD: beta = texture[3]; break;        // SST_CC_MATREX: texture alpha
  case SST_TC_MLODFRAC: factors = texture; break;    // SST_CC_MRGBTMU: texture color
  case SST_TC_MCMSELECT7: factors = mselect7; break;
  }
  for (int i = 0; i < 3; i++) {
    const int factor = (factors != nullptr) ? factors[i] : beta;
    if (factor >= 0) c[i] *= (factor ^ reverse) + 1;
  }

  int add[3] = {0, 0, 0};
  switch ((((mode & SST_TC_ADD_ALOCAL) != 0) << 0) | (((mode & SST_TC_ADD_CLOCAL) != 0) << 1)) {
  case 1:  // local alpha
    add[0] = add[1] = add[2] = local[3];
    break;
  case 2:  // local color
    for (int i = 0; i < 3; i++) add[i] = local[i];
    break;
  case 3:  // texture color
    for (int i = 0; i < 3; i++) add[i] = texture[i];
    break;
  }

  for (int i = 0; i < 3; i++) {
    c[i] >>= 8;  // back to 8.0
    if (combineMode & SST_CM_TC_INVERT_ADD_LOCAL) add[i] = ~add[i] & 0xFF;
    c[i] = outShift(c[i] + add[i], combineMode & SST_CM_TC_OUTSHIFT, SST_CM_TC_OUTSHIFT_2X,
                    SST_CM_TC_OUTSHIFT_4X);
    c[i] = std::clamp(c[i], 0, 255);
    if (mode & SST_TC_INVERT_OUTPUT) c[i] ^= 0xFF;
    local[i] = static_cast<FxU8>(c[i]);
  }
}

void combineAlpha(FxU8 local[4], const FxU8 other[4], const FxU8 iterated[4],
                  const FxU8 texture[4], const CombineConfig &config)
{
  const FxU32 mode = config.mode;
  const FxU32 combineMode = config.combineMode;
  const int reverse = static_cast<int>(config.alphaReverse);

  int otherTemp = (mode & SST_TCA_ZERO_OTHER) ? 0 : other[3];
  int localTemp = (mode & SST_TCA_SUB_CLOCAL) ? local[3] : 0;
  otherTemp = invert(otherTemp, combineMode & SST_CM_TCA_INVERT_OTHER,
                     SST_CM_TCA_INVERT_OTHER_ZERO_MINUS_X, SST_CM_TCA_INVERT_OTHER_ONE_MINUS_X,
                     SST_CM_TCA_INVERT_OTHER_X_MINUS_HALF);
  localTemp = invert(localTemp, combineMode & SST_CM_TCA_INVERT_LOCAL,
                     SST_CM_TCA_INVERT_LOCAL_ZERO_MINUS_X, SST_CM_TCA_INVERT_LOCAL_ONE_MINUS_X,
                     SST_CM_TCA_INVERT_LOCAL_X_MINUS_HALF);
  int a = otherTemp + localTemp;

  int beta = -1;
  switch (mode & SST_TCA_MSELECT) {
  case SST_TCA_MONE: beta = 0; break;
  case SST_TCA_MCLOCAL: beta = local[3]; break;
  case SST_TCA_MAOTHER: beta = other[3]; break;
  case SST_TCA_MALOCAL: beta = local[3]; break;
  case SST_TCA_MLOD: beta = texture[3]; break;     // SST_CCA_MATREX: texture alpha
  case SST_TCA_MLODFRAC: beta = iterated[3]; break;  // SST_CCA_MAITER: iterated alpha
  case SST_TCA_MAITER: beta = config.color1Alpha; break;
  case SST_TCA_MCR: beta = 0; break;
  }
  if (beta >= 0) a *= (beta ^ reverse) + 1;
  a >>= 8;

  int add = 0;
  switch ((((mode & SST_TCA_ADD_ALOCAL) != 0) << 0) | (((mode & SST_TCA_ADD_CLOCAL) != 0) << 1)) {
  case 1:
  case 2:
    add = local[3];
    break;
  case 3:  // texture alpha
    add = texture[3];
    break;
  }
  if (combineMode & SST_CM_TCA_INVERT_ADD_LOCAL) add = ~add & 0xFF;

  a = outShift(a + add, combineMode & SST_CM_TCA_OUTSHIFT, SST_CM_TCA_OUTSHIFT_2X,
               SST_CM_TCA_OUTSHIFT_4X);
  a = std::clamp(a, 0, 255);
  if (mode & SST_TCA_INVERT_OUTPUT) a ^= 0xFF;
  local[3] = static_cast<FxU8>(a);
}

}  // namespace csim
