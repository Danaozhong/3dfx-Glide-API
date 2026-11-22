#include <algorithm>
#include <cmath>

#include "fixed_point.hpp"
#include "texture_unit.hpp"

namespace csim {

namespace {

/// @brief Log2 of the larger side of LOD 0.
constexpr int kLod0Log2 = 8;
/// @brief Bound of the computed LOD, in 1/256 levels.
constexpr int kLodLimit = 32 << 8;
/// @brief Bound of the texture coordinates in texels, far outside of any texture.
constexpr double kCoordinateLimit = 1099511627776.0;  // 2^40
/// @brief Value of one unit of the S and T iterators.
constexpr double kStUnit = 1.0 / (1 << SST_ST_FRACBITS);
/// @brief Value of one unit of the W iterator.
constexpr double kWUnit = 1.0 / (1 << SST_W_FRACBITS);

/**
 * @brief A texture format of textureMode, shifted down.
 * @param[in] format The SST_TFORMAT value.
 */
constexpr FxU32 fmt(FxU32 format) { return format >> SST_TFORMAT_SHIFT; }

/**
 * @brief Bytes of a texel.
 * @param[in] format The texture format, see fmt().
 */
constexpr FxU32 bytesPerTexel(FxU32 format)
{
  return (format < fmt(SST_ARGB8332)) ? 1 : (format < fmt(SST_ARGB8888)) ? 2 : 4;
}

/**
 * @brief Expands a channel to 8 bits by repeating its bits.
 * @param[in] value The channel.
 * @param[in] bits Width of the channel, 1 to 8.
 */
constexpr FxU32 expand(FxU32 value, int bits)
{
  FxU32 r = (value & mask(bits)) << (8 - bits);
  for (int n = bits; n < 8; n *= 2) r |= r >> n;
  return r & 0xFF;
}

/**
 * @brief Brings a texel coordinate into a mipmap level.
 * @param[in] i The coordinate.
 * @param[in] size The side of the level, a power of 2.
 * @param[in] clamp Clamp to the edge instead of wrapping around.
 * @param[in] mirror Mirror every other repetition; takes precedence over
 * @p clamp.
 */
int wrapCoordinate(FxI64 i, int size, bool clamp, bool mirror)
{
  if (mirror) {
    const FxI64 m = i & (2 * size - 1);
    return static_cast<int>((m < size) ? m : 2 * size - 1 - m);
  }
  if (clamp) return static_cast<int>(std::clamp<FxI64>(i, 0, size - 1));
  return static_cast<int>(i & (size - 1));
}

/**
 * @brief A texture coordinate in 1/256 texels.
 * @param[in] texels The coordinate in texels.
 */
FxI64 subTexels(double texels)
{
  if (std::isnan(texels)) return 0;
  return static_cast<FxI64>(std::floor(std::clamp(texels, -kCoordinateLimit, kCoordinateLimit) * 256));
}

/**
 * @brief Copies the color or the alpha of a color into the RGB of another.
 * @param[in] color The color, r, g, b, a.
 * @param[in] alpha Copy the alpha into all three channels.
 * @param[out] out Receives r, g and b.
 */
void rgbOf(const FxU8 color[4], bool alpha, FxU8 out[3])
{
  for (int i = 0; i < 3; i++) out[i] = alpha ? color[3] : color[i];
}

}  // namespace

TextureState TextureState::decode(const SstRegs &tmu)
{
  TextureState r;
  const FxU32 tm = tmu.textureMode;
  const FxU32 lod = tmu.tLOD;
  const FxU32 detail = tmu.tDetail;
  const FxU32 cm = tmu.combineMode;

  r.perspective = (tm & SST_TPERSP_ST) != 0;
  r.clampW = (tm & SST_TCLAMPW) != 0;
  r.clampS = (tm & SST_TCLAMPS) != 0;
  r.clampT = (tm & SST_TCLAMPT) != 0;
  r.mirrorS = (lod & SST_TMIRRORS) != 0;
  r.mirrorT = (lod & SST_TMIRRORT) != 0;

  // The LOD fields are 4.2.
  r.lodMin = static_cast<int>((lod & SST_LODMIN) >> SST_LODMIN_SHIFT) << 6;
  r.lodMax = static_cast<int>((lod & SST_LODMAX) >> SST_LODMAX_SHIFT) << 6;
  r.lodBias = signExtend((lod & SST_LODBIAS) >> SST_LODBIAS_SHIFT, 6) * 64;
  r.aspect = static_cast<int>((lod & SST_LOD_ASPECT) >> SST_LOD_ASPECT_SHIFT);
  r.sIsWider = (lod & SST_LOD_S_IS_WIDER) != 0;

  if (detail & SST_TFILTER_SEPARATE) {
    r.minBilinearRgb = (detail & SST_TMINFILTER_RGB) != 0;
    r.magBilinearRgb = (detail & SST_TMAGFILTER_RGB) != 0;
    r.minBilinearAlpha = (detail & SST_TMINFILTER_A) != 0;
    r.magBilinearAlpha = (detail & SST_TMAGFILTER_A) != 0;
  } else {
    r.minBilinearRgb = r.minBilinearAlpha = (tm & SST_TMINFILTER) != 0;
    r.magBilinearRgb = r.magBilinearAlpha = (tm & SST_TMAGFILTER) != 0;
  }

  r.format = fmt(tm & SST_TFORMAT);
  r.base = SST_TEXTURE_UNMUNGE_ADDRESS(tmu.texBaseAddr);
  r.ncc1 = (tm & SST_TNCCSELECT) != 0;

  r.detailBias = signExtend((detail & SST_DETAIL_BIAS) >> SST_DETAIL_BIAS_SHIFT, 6);
  r.detailScale = static_cast<int>((detail & SST_DETAIL_SCALE) >> SST_DETAIL_SCALE_SHIFT);
  r.detailMax = static_cast<int>((detail & SST_DETAIL_MAX) >> SST_DETAIL_MAX_SHIFT);

  r.useCombineMode = (cm & SST_CM_USE_COMBINE_MODE) != 0;
  r.combine.mode = tm & (SST_TCOMBINE | SST_TACOMBINE);
  r.combine.rgbReverse = (tm & SST_TC_REVERSE_BLEND) ? 0x00 : 0xFF;
  r.combine.alphaReverse = (tm & SST_TCA_REVERSE_BLEND) ? 0x00 : 0xFF;
  r.combine.combineMode = r.useCombineMode ? cm : 0;
  r.usesOther = r.useCombineMode || !(tm & SST_TC_ZERO_OTHER) || !(tm & SST_TCA_ZERO_OTHER) ||
                ((tm & SST_TC_MSELECT) == SST_TC_MAOTHER) || ((tm & SST_TCA_MSELECT) == SST_TCA_MAOTHER);

  bool chroma = (tm & SST_TCA_MSELECT) == SST_TCA_MCR;
  if (r.useCombineMode) {
    chroma = chroma || ((cm & SST_CM_TC_LOCALSELECT) >= SST_CM_TC_LOCALSELECT_CK_RGB) ||
             ((cm & SST_CM_TC_OTHERSELECT) >= SST_CM_TC_OTHERSELECT_CR_RGB) ||
             ((cm & SST_CM_TC_MSELECT_7) >= SST_CM_TC_MSELECT_7_CR_RGB) ||
             ((cm & SST_CM_TCA_LOCALSELECT) == SST_CM_TCA_LOCALSELECT_CK_A) ||
             ((cm & SST_CM_TCA_OTHERSELECT) == SST_CM_TCA_OTHERSELECT_CR_A);
  }
  r.needsUnsimulated = (tm & (SST_TRILINEAR | SST_COMPRESSED_TEXTURES | SST_TLODDITHER)) ||
                       (lod & (SST_LOD_TSPLIT | SST_TMULTIBASEADDR | SST_TBIG)) ||
                       (tmu.texBaseAddr & SST_TEXTURE_IS_TILED) || (r.format == fmt(SST_ARGB8332) - 1) ||
                       chroma;
  return r;
}

void TextureUnit::storePalette(int reg, FxU32 data)
{
  palette_[((data >> 23) & 0xFE) | (static_cast<FxU32>(reg) & 1)] = data & 0x00FFFFFF;
}

void TextureUnit::beginTriangle()
{
  const SstRegs &t = context_.tmu(index_);
  state_ = TextureState::decode(t);

  dsdx_ = t.dsdx * kStUnit;
  dtdx_ = t.dtdx * kStUnit;
  dwdx_ = t.dwdx * kWUnit;
  dsdy_ = t.dsdy * kStUnit;
  dtdy_ = t.dtdy * kStUnit;
  dwdy_ = t.dwdy * kWUnit;

  // A YIQ texel is 4 bits Y, 2 bits I and 2 bits Q. The table has 16 Y
  // bytes in 4 words, then 4 I and 4 Q words of 9-bit signed r, g, b.
  if ((state_.format == fmt(SST_YIQ422)) || (state_.format == fmt(SST_AYIQ8422))) {
    const volatile FxU32 *ncc = state_.ncc1 ? t.nccTable1 : t.nccTable0;
    for (int i = 0; i < 256; i++) {
      const int y = static_cast<int>((ncc[i >> 6] >> (8 * ((i >> 4) & 3))) & 0xFF);
      const FxU32 iq[2] = {ncc[4 + ((i >> 2) & 3)], ncc[8 + (i & 3)]};
      FxU32 color = 0;
      for (int c = 0; c < 3; c++) {
        const int shift = 18 - 9 * c;
        const int value = y + signExtend(iq[0] >> shift, 9) + signExtend(iq[1] >> shift, 9);
        color |= static_cast<FxU32>(std::clamp(value, 0, 255)) << (16 - 8 * c);
      }
      ncc_[i] = color;
    }
  }
}

TextureUnit::Level TextureUnit::level(int lod) const
{
  auto sides = [&](int l, int &width, int &height) {
    const int wide = std::max(1, (1 << kLod0Log2) >> l);
    const int narrow = std::max(1, ((1 << kLod0Log2) >> state_.aspect) >> l);
    width = state_.sIsWider ? wide : narrow;
    height = state_.sIsWider ? narrow : wide;
  };

  FxU32 texels = 0;
  for (int l = 0; l < lod; l++) {
    int width, height;
    sides(l, width, height);
    texels += static_cast<FxU32>(width * height);
  }
  Level result;
  sides(lod, result.width, result.height);
  result.address = state_.base + texels * bytesPerTexel(state_.format);
  return result;
}

void TextureUnit::texel(const Level &level, FxI64 s, FxI64 t, FxU8 out[4]) const
{
  const FxU32 bytes = bytesPerTexel(state_.format);
  const int col = wrapCoordinate(s, level.width, state_.clampS, state_.mirrorS);
  const int row = wrapCoordinate(t, level.height, state_.clampT, state_.mirrorT);
  const FxU32 addr = level.address + static_cast<FxU32>(row * level.width + col) * bytes;

  FxU32 raw = 0;
  if ((addr < context_.fbSize) && (context_.fbSize - addr >= bytes)) {
    const FxU8 *p = context_.frameBuffer() + addr;
    for (FxU32 i = 0; i < bytes; i++) raw |= FxU32{p[i]} << (8 * i);
  }
  decode(raw, out);
}

void TextureUnit::decode(FxU32 raw, FxU8 out[4]) const
{
  FxU32 a = 0xFF, r = 0, g = 0, b = 0;
  auto rgb = [&](FxU32 color) {
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
  };
  auto rgb332 = [&](FxU32 c) {
    r = expand(c >> 5, 3);
    g = expand(c >> 2, 3);
    b = expand(c, 2);
  };

  switch (state_.format) {
  case fmt(SST_RGB332): rgb332(raw); break;
  case fmt(SST_YIQ422): rgb(ncc_[raw & 0xFF]); break;
  case fmt(SST_A8): a = r = g = b = raw & 0xFF; break;
  case fmt(SST_I8): r = g = b = raw & 0xFF; break;
  case fmt(SST_AI44):
    a = expand(raw >> 4, 4);
    r = g = b = expand(raw, 4);
    break;
  case fmt(SST_P8): rgb(palette_[raw & 0xFF]); break;
  case fmt(SST_P8_ARGB6666): {
    const FxU32 c = palette_[raw & 0xFF];
    a = expand(c >> 18, 6);
    r = expand(c >> 12, 6);
    g = expand(c >> 6, 6);
    b = expand(c, 6);
    break;
  }
  case fmt(SST_ARGB8332):
    a = (raw >> 8) & 0xFF;
    rgb332(raw);
    break;
  case fmt(SST_AYIQ8422):
    a = (raw >> 8) & 0xFF;
    rgb(ncc_[raw & 0xFF]);
    break;
  case fmt(SST_RGB565):
    r = expand(raw >> 11, 5);
    g = expand(raw >> 5, 6);
    b = expand(raw, 5);
    break;
  case fmt(SST_ARGB1555):
    a = (raw & 0x8000) ? 0xFF : 0x00;
    r = expand(raw >> 10, 5);
    g = expand(raw >> 5, 5);
    b = expand(raw, 5);
    break;
  case fmt(SST_ARGB4444):
    a = expand(raw >> 12, 4);
    r = expand(raw >> 8, 4);
    g = expand(raw >> 4, 4);
    b = expand(raw, 4);
    break;
  case fmt(SST_AI88):
    a = (raw >> 8) & 0xFF;
    r = g = b = raw & 0xFF;
    break;
  case fmt(SST_AP88):
    a = (raw >> 8) & 0xFF;
    rgb(palette_[raw & 0xFF]);
    break;
  case fmt(SST_ARGB8888):
    a = raw >> 24;
    rgb(raw);
    break;
  default:  // format 7 does not exist
    a = 0;
    break;
  }
  out[0] = static_cast<FxU8>(r);
  out[1] = static_cast<FxU8>(g);
  out[2] = static_cast<FxU8>(b);
  out[3] = static_cast<FxU8>(a);
}

void TextureUnit::shade(const TexCoords &coords, const FxU8 iterated[4], const FxU8 other[4],
                        FxU8 out[4]) const
{
  // The texel position and its steps to the next pixel in x and y, in texels
  // of LOD 0.
  const double sw = static_cast<double>(coords.s) * kStUnit;
  const double tw = static_cast<double>(coords.t) * kStUnit;
  const double w = static_cast<double>(coords.w) * kWUnit;
  double s = sw, t = tw;
  double dsdx = dsdx_, dtdx = dtdx_, dsdy = dsdy_, dtdy = dtdy_;
  if (state_.perspective) {
    if ((w == 0) || ((w < 0) && state_.clampW)) {
      s = t = dsdx = dtdx = dsdy = dtdy = 0;
    } else {
      // d(S/W) = (dS - S/W dW) / W
      s = sw / w;
      t = tw / w;
      dsdx = (dsdx_ - s * dwdx_) / w;
      dtdx = (dtdx_ - t * dwdx_) / w;
      dsdy = (dsdy_ - s * dwdy_) / w;
      dtdy = (dtdy_ - t * dwdy_) / w;
    }
  }

  // The LOD is log2 of the larger step, with 8 fraction bits.
  const double rho2 = std::max(dsdx * dsdx + dtdx * dtdx, dsdy * dsdy + dtdy * dtdy);
  int lod = -kLodLimit;
  if (rho2 > 0) {
    lod = static_cast<int>(
        std::clamp(std::floor(128.0 * std::log2(rho2)), -static_cast<double>(kLodLimit),
                   static_cast<double>(kLodLimit)));
  }
  lod += state_.lodBias;
  const bool magnify = lod < state_.lodMin;
  const int clamped = std::min(std::max(lod, state_.lodMin), state_.lodMax);
  const int lodLevel = clamped >> 8;
  const auto lodFrac = static_cast<FxU8>(clamped & 0xFF);
  const auto detail = static_cast<FxU8>(std::clamp(
      (state_.detailBias - (lod >> 6)) * (1 << state_.detailScale), 0, state_.detailMax));

  // Sample the level, texel centers are at .5.
  const Level lv = level(lodLevel);
  const FxI64 u = subTexels(s) >> lodLevel;
  const FxI64 v = subTexels(t) >> lodLevel;
  const bool bilinearRgb = magnify ? state_.magBilinearRgb : state_.minBilinearRgb;
  const bool bilinearAlpha = magnify ? state_.magBilinearAlpha : state_.minBilinearAlpha;
  FxU8 tex[4] = {};
  if (bilinearRgb || bilinearAlpha) {
    const FxI64 u0 = u - 128, v0 = v - 128;
    const FxI64 i = u0 >> 8, j = v0 >> 8;
    const int fu = static_cast<int>(u0 & 0xFF), fv = static_cast<int>(v0 & 0xFF);
    FxU8 c[4][4];
    texel(lv, i, j, c[0]);
    texel(lv, i + 1, j, c[1]);
    texel(lv, i, j + 1, c[2]);
    texel(lv, i + 1, j + 1, c[3]);
    for (int k = 0; k < 4; k++) {
      const int top = c[0][k] * (256 - fu) + c[1][k] * fu;
      const int bottom = c[2][k] * (256 - fu) + c[3][k] * fu;
      tex[k] = static_cast<FxU8>((top * (256 - fv) + bottom * fv) >> 16);
    }
  }
  if (!bilinearRgb || !bilinearAlpha) {
    FxU8 p[4];
    texel(lv, u >> 8, v >> 8, p);
    if (!bilinearRgb) rgbOf(p, false, tex);
    if (!bilinearAlpha) tex[3] = p[3];
  }

  // The texture combine unit.
  FxU8 local[4] = {tex[0], tex[1], tex[2], tex[3]};
  FxU8 otherColor[4] = {other[0], other[1], other[2], other[3]};
  CombineInputs inputs;
  rgbOf(tex, false, inputs.mselect7);
  if (state_.useCombineMode) {
    // Chroma key and chroma range inputs read as 0.
    static constexpr FxU8 kZero[4] = {};
    const FxU32 cm = state_.combine.combineMode;
    switch (cm & SST_CM_TC_LOCALSELECT) {
    case SST_CM_TC_LOCALSELECT_LOCAL_TRGB: rgbOf(tex, false, local); break;
    case SST_CM_TC_LOCALSELECT_LOCAL_TA: rgbOf(tex, true, local); break;
    case SST_CM_TC_LOCALSELECT_OTHER_TRGB: rgbOf(other, false, local); break;
    case SST_CM_TC_LOCALSELECT_OTHER_TA: rgbOf(other, true, local); break;
    case SST_CM_TC_LOCALSELECT_IRGB: rgbOf(iterated, false, local); break;
    case SST_CM_TC_LOCALSELECT_IA: rgbOf(iterated, true, local); break;
    default: rgbOf(kZero, false, local); break;
    }
    switch (cm & SST_CM_TC_OTHERSELECT) {
    case SST_CM_TC_OTHERSELECT_OTHER_TRGB: rgbOf(other, false, otherColor); break;
    case SST_CM_TC_OTHERSELECT_OTHER_TA: rgbOf(other, true, otherColor); break;
    case SST_CM_TC_OTHERSELECT_LOCAL_TRGB: rgbOf(tex, false, otherColor); break;
    case SST_CM_TC_OTHERSELECT_LOCAL_TA: rgbOf(tex, true, otherColor); break;
    case SST_CM_TC_OTHERSELECT_IRGB: rgbOf(iterated, false, otherColor); break;
    case SST_CM_TC_OTHERSELECT_IA: rgbOf(iterated, true, otherColor); break;
    default: rgbOf(kZero, false, otherColor); break;
    }
    switch (cm & SST_CM_TC_MSELECT_7) {
    case SST_CM_TC_MSELECT_7_LOCAL_TRGB: break;
    case SST_CM_TC_MSELECT_7_OTHER_TRGB: rgbOf(other, false, inputs.mselect7); break;
    case SST_CM_TC_MSELECT_7_IRGB: rgbOf(iterated, false, inputs.mselect7); break;
    case SST_CM_TC_MSELECT_7_IA: rgbOf(iterated, true, inputs.mselect7); break;
    default: rgbOf(kZero, false, inputs.mselect7); break;
    }
    switch (cm & SST_CM_TCA_LOCALSELECT) {
    case SST_CM_TCA_LOCALSELECT_LOCAL_TA: local[3] = tex[3]; break;
    case SST_CM_TCA_LOCALSELECT_OTHER_TA: local[3] = other[3]; break;
    case SST_CM_TCA_LOCALSELECT_IA: local[3] = iterated[3]; break;
    default: local[3] = 0; break;
    }
    switch (cm & SST_CM_TCA_OTHERSELECT) {
    case SST_CM_TCA_OTHERSELECT_OTHER_TA: otherColor[3] = other[3]; break;
    case SST_CM_TCA_OTHERSELECT_LOCAL_TA: otherColor[3] = tex[3]; break;
    case SST_CM_TCA_OTHERSELECT_IA: otherColor[3] = iterated[3]; break;
    default: otherColor[3] = 0; break;
    }
  }
  for (int i = 0; i < 3; i++) {
    inputs.mlod[i] = detail;
    inputs.mlodFrac[i] = lodFrac;
    inputs.addBoth[i] = iterated[i];
  }
  inputs.alphaMlod = detail;
  inputs.alphaMlodFrac = lodFrac;
  inputs.alphaMaiter = iterated[3];
  inputs.alphaAddBoth = iterated[3];

  combineColor(local, otherColor, inputs, state_.combine);
  combineAlpha(local, otherColor, inputs, state_.combine);
  for (int i = 0; i < 4; i++) out[i] = local[i];
}

}  // namespace csim
