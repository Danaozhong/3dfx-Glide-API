/**
 * @file
 * @brief Color and alpha combine units.
 *
 * The FBI and the TMUs have the same units, with the mode bits in the TMU
 * layout (SST_TC_*, SST_TCA_*). Colors are FxU8[4] in r, g, b, a order.
 */
#ifndef CSIM_COLOR_COMBINE_HPP
#define CSIM_COLOR_COMBINE_HPP

#include "render_state.hpp"

namespace csim {

/**
 * @brief The inputs of the combine units besides the local and the other
 * color.
 *
 * The mode bits select them the same way in the FBI and in the TMUs, but
 * they are different values there.
 */
struct CombineInputs {
  FxU8 mlod[3] = {};      ///< SST_TC_MLOD: texture alpha in the FBI, the detail factor in a TMU.
  FxU8 mlodFrac[3] = {};  ///< SST_TC_MLODFRAC: texture color in the FBI, the LOD fraction in a TMU.
  FxU8 mselect7[3] = {};  ///< SST_TC_MCMSELECT7, chosen by combineMode.
  /// Added with SST_TC_ADD_CLOCAL and SST_TC_ADD_ALOCAL: texture color in
  /// the FBI, iterated color in a TMU.
  FxU8 addBoth[3] = {};
  FxU8 alphaMlod = 0;      ///< SST_TCA_MLOD: texture alpha in the FBI, the detail factor in a TMU.
  FxU8 alphaMlodFrac = 0;  ///< SST_TCA_MLODFRAC: iterated alpha in the FBI, the LOD fraction in a TMU.
  FxU8 alphaMaiter = 0;    ///< SST_TCA_MAITER: color 1 alpha in the FBI, iterated alpha in a TMU.
  FxU8 alphaMcr = 0;       ///< SST_TCA_MCR: 0 in the FBI, chroma range alpha in a TMU.
  /// Added with SST_TCA_ADD_CLOCAL and SST_TCA_ADD_ALOCAL: texture alpha in
  /// the FBI, iterated alpha in a TMU.
  FxU8 alphaAddBoth = 0;
};

/**
 * @brief The color combine unit.
 * @param[in,out] local The local color; receives the combined color.
 * @param[in] other The other color.
 * @param[in] inputs The other inputs.
 * @param[in] config The mode bits.
 */
void combineColor(FxU8 local[4], const FxU8 other[4], const CombineInputs &inputs,
                  const CombineConfig &config);

/**
 * @brief The alpha combine unit.
 * @param[in,out] local The local color; its alpha receives the combined alpha.
 * @param[in] other The other color.
 * @param[in] inputs The other inputs.
 * @param[in] config The mode bits.
 */
void combineAlpha(FxU8 local[4], const FxU8 other[4], const CombineInputs &inputs,
                  const CombineConfig &config);

}  // namespace csim

#endif  // CSIM_COLOR_COMBINE_HPP
