#include "color_combine.hpp"
#include "fixed_point.hpp"
#include "fbi_pixel_pipeline.hpp"

namespace csim {

namespace {

/// @brief The 4x4 ordered dither matrix.
constexpr int kDitherMatrix[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/**
 * @brief The kDepthBits pixel depth from the Z iterator.
 *
 * In 16bpp the low 8 bits are fraction and dropped, so the depth moves in
 * steps of 256.
 *
 * @param[in] z64 The Z iterator.
 * @param[in] is32Bpp The color buffer is 32bpp.
 * @param[in] clamp Saturate instead of wrapping, see saturateOrWrap().
 */
FxU32 clampDepth(FxI64 z64, bool is32Bpp, bool clamp)
{
  const FxI32 step = is32Bpp ? 1 : 1 << 8;
  const FxI32 z = signExtend(static_cast<FxU32>(z64 >> kZIteratorFracBits32Bpp),
                             kDepthBits + kIteratorOverflowBits) &
                  -step;
  return saturateOrWrap(z, (1 << kDepthBits) - step, step, clamp);
}

/**
 * @brief The chroma key test.
 * @param[in] r Red, 8 bits.
 * @param[in] g Green, 8 bits.
 * @param[in] b Blue, 8 bits.
 * @param[in] key The chromaKey register: the key, or the low end of the range.
 * @param[in] range The chromaRange register: the high end and the mode.
 * @return True if the pixel is discarded.
 */
bool chromaTest(FxU32 r, FxU32 g, FxU32 b, FxU32 key, FxU32 range)
{
  if (!(range & SST_ENCHROMARANGE)) return ((r << 16) | (g << 8) | b) == (key & 0x00FFFFFF);

  bool rPass = (r >= ((key >> 16) & 0xFF)) && (r <= ((range >> 16) & 0xFF));
  bool gPass = (g >= ((key >> 8) & 0xFF)) && (g <= ((range >> 8) & 0xFF));
  bool bPass = (b >= (key & 0xFF)) && (b <= (range & 0xFF));
  if (range & SST_CHROMARANGE_RED_EX) rPass = !rPass;
  if (range & SST_CHROMARANGE_GREEN_EX) gPass = !gPass;
  if (range & SST_CHROMARANGE_BLUE_EX) bPass = !bPass;
  if (range & SST_CHROMARANGE_BLOCK_OR) return rPass || gPass || bPass;
  return rPass && gPass && bPass;
}

/**
 * @brief Dithers an 8-bit channel to 5 bits.
 * @param[in] v The channel.
 * @param[in] d The value of the dither matrix, 0 to 15.
 */
FxU32 dither5(FxU32 v, int d) { return ((v << 1) - (v >> 4) + (v >> 7) + d) >> 4; }
/**
 * @brief Dithers an 8-bit channel to 6 bits.
 * @param[in] v The channel.
 * @param[in] d The value of the dither matrix, 0 to 15.
 */
FxU32 dither6(FxU32 v, int d) { return ((v << 2) - (v >> 4) + (v >> 6) + d) >> 4; }

/**
 * @brief Increments a 24-bit statistics register.
 * @param[in,out] counter The register.
 */
void increment(volatile FxU32 &counter) { counter = (counter + 1) & 0xFFFFFF; }

/// @brief The inputs of the color path for one pixel.
struct Colors {
  FxU8 iterated[4];
  const FxU8 *texture;
  FxU32 color0, color1;
  FxU8 depthAlpha;  ///< High bits of the pixel depth.

  /**
   * @brief The color of an input of the color combine unit.
   * @param[in] source The input.
   * @param[out] out Red, green and blue.
   */
  void rgb(RgbSource source, FxU8 out[3]) const
  {
    auto channels = [&](FxU32 color) {
      out[0] = static_cast<FxU8>(color >> 16);
      out[1] = static_cast<FxU8>(color >> 8);
      out[2] = static_cast<FxU8>(color);
    };
    auto all = [&](FxU32 value) { out[0] = out[1] = out[2] = static_cast<FxU8>(value); };
    switch (source) {
    case RgbSource::Iterated: out[0] = iterated[0]; out[1] = iterated[1]; out[2] = iterated[2]; break;
    case RgbSource::Texture: out[0] = texture[0]; out[1] = texture[1]; out[2] = texture[2]; break;
    case RgbSource::Color0: channels(color0); break;
    case RgbSource::Color1: channels(color1); break;
    case RgbSource::IteratedAlpha: all(iterated[3]); break;
    case RgbSource::TextureAlpha: all(texture[3]); break;
    case RgbSource::Color0Alpha: all(color0 >> 24); break;
    case RgbSource::Color1Alpha: all(color1 >> 24); break;
    case RgbSource::Zero: all(0); break;
    }
  }

  /**
   * @brief The alpha of an input of the alpha combine unit.
   * @param[in] source The input.
   */
  FxU8 alpha(AlphaSource source) const
  {
    switch (source) {
    case AlphaSource::Iterated: return iterated[3];
    case AlphaSource::Texture: return texture[3];
    case AlphaSource::Color0: return static_cast<FxU8>(color0 >> 24);
    case AlphaSource::Color1: return static_cast<FxU8>(color1 >> 24);
    case AlphaSource::Z: return depthAlpha;
    case AlphaSource::W: return 0;  // W is not simulated
    case AlphaSource::Zero: return 0;
    }
    return 0;
  }
};

}  // namespace

void FbiPixelPipeline::shade(const Fragment &fragment, const RenderState &state)
{
  SstRegs &s = context_.sst();
  const int x = fragment.x;
  const int y = state.flipY ? state.yOrigin - fragment.y : fragment.y;
  const bool is32Bpp = state.format == PixelFormat::Argb8888;

  Colors colors;
  colors.iterated[0] = clampIterator(fragment.r, state.clampIterators);
  colors.iterated[1] = clampIterator(fragment.g, state.clampIterators);
  colors.iterated[2] = clampIterator(fragment.b, state.clampIterators);
  colors.iterated[3] = clampIterator(fragment.a, state.clampIterators);
  colors.texture = fragment.texture;
  colors.color0 = state.color0;
  colors.color1 = state.color1;
  FxU32 z = clampDepth(fragment.z64, is32Bpp, state.clampIterators);
  colors.depthAlpha = static_cast<FxU8>(z >> (kDepthBits - 8));

  FxU8 other[4];
  colors.rgb(state.otherRgb, other);
  other[3] = colors.alpha(state.otherAlpha);

  // The alpha mask fails after the chroma key, like the hardware.
  const bool alphaMaskFailed = state.alphaMask && !(other[3] & 1);
  if (alphaMaskFailed) increment(s.stats.fbiAfuncFail);
  if (state.chromaKey &&
      chromaTest(other[0], other[1], other[2], state.chromaKeyColor, state.chromaRange)) {
    increment(s.stats.fbiChromaFail);
    return;
  }
  if (alphaMaskFailed) return;

  CombineInputs inputs;
  colors.rgb(state.mselect7, inputs.mselect7);
  for (int i = 0; i < 3; i++) {
    inputs.mlod[i] = colors.texture[3];
    inputs.mlodFrac[i] = colors.texture[i];
    inputs.addBoth[i] = colors.texture[i];
  }
  inputs.alphaMlod = colors.texture[3];
  inputs.alphaMlodFrac = colors.iterated[3];
  inputs.alphaMaiter = static_cast<FxU8>(colors.color1 >> 24);
  inputs.alphaAddBoth = colors.texture[3];

  FxU8 local[4];
  if (state.localRgbFromTextureAlpha) {
    colors.rgb((colors.texture[3] & 0x80) ? RgbSource::Color0 : RgbSource::Iterated, local);
  } else {
    colors.rgb(state.localRgb, local);
  }
  local[3] = colors.alpha(state.localAlpha);

  combineColor(local, other, inputs, state.combine);
  combineAlpha(local, other, inputs, state.combine);

  if (state.alphaTest && !state.alphaFunc.passes(local[3], state.alphaRef)) {
    increment(s.stats.fbiAfuncFail);
    return;
  }

  if (state.depthBias) {
    z += static_cast<FxU32>(state.depthBiasValue);
    if (z & 0x80000000) z = 0;
  }
  if (z > 0xFFFFFF) z = 0xFFFFFF;
  if (state.depthTest) {
    FxU32 ztest, zref;
    if (is32Bpp) {
      ztest = state.depthFromZaColor ? (state.zaColor & 0xFFFFFF) : z;
      zref = frameBuffer_.readPixel(state.aux, x, y) & 0x00FFFFFF;
    } else {
      ztest = state.depthFromZaColor ? (state.zaColor & 0xFFFF) : (z >> 8);
      zref = frameBuffer_.readPixel(state.aux, x, y);
    }
    if (!state.depthFunc.passes(ztest, zref)) {
      increment(s.stats.fbiZfuncFail);
      return;
    }
  }

  if (is32Bpp) {
    // Stencil and depth share the aux buffer, the stencil gets the reference
    // value through its write mask.
    const FxU32 old = frameBuffer_.readPixel(state.aux, x, y);
    const FxU32 stencil =
        (((old >> 24) & ~state.stencilWriteMask) | (state.stencilRef & state.stencilWriteMask)) & 0xFF;
    z = (stencil << 24) | ((state.auxWrite ? z : old) & 0x00FFFFFF);
  }
  ditherAndWrite(x, y, local[0], local[1], local[2], local[3], z, state);
}

void FbiPixelPipeline::fill(int x, int y, const RenderState &state)
{
  if (state.flipY) y = state.yOrigin - y;
  const FxU32 c1 = state.color1;
  const FxU32 r = (c1 >> 16) & 0xFF, g = (c1 >> 8) & 0xFF, b = c1 & 0xFF;

  if (state.format == PixelFormat::Argb8888) {
    FxU32 z = (state.zaColor & 0x00FFFFFF) | (state.stencilRef << 24);
    if (state.auxWrite || state.stencilWriteMask) {
      const FxU32 old = frameBuffer_.readPixel(state.aux, x, y);
      if (!state.auxWrite) z = (z & 0xFF000000) | (old & 0x00FFFFFF);
      if (!state.stencilWriteMask) z = (z & 0x00FFFFFF) | (old & 0xFF000000);
    }
    ditherAndWrite(x, y, r, g, b, c1 >> 24, z, state);
  } else {
    ditherAndWrite(x, y, r, g, b, state.zaColor >> 24, (state.zaColor & 0xFFFF) << 8, state);
  }
}

void FbiPixelPipeline::lfbWrite(int x, int y, const LfbPixel &pixel, bool pixelPipeline,
                                const RenderState &state)
{
  RenderState writes = state;
  writes.rgbWrite = pixel.hasColor && (state.rgbWrite || !pixelPipeline);
  writes.auxWrite = pixel.hasDepth && (state.auxWrite || !pixelPipeline);
  writes.alphaBuffer = false;
  if (pixelPipeline) {
    context_.stats->unsimulated++;
  } else {
    // Bypassing the pipeline, the pixel is stored as it is.
    writes.dither = Dither::None;
    writes.colorWriteMask = 0xFFFFFFFF;
    writes.alpha1555 = Alpha1555::Source;
    writes.stencilWriteMask = 0;
  }
  if (!writes.rgbWrite && !writes.auxWrite) return;

  FxU32 z = pixel.z;
  if ((state.format == PixelFormat::Argb8888) && !writes.auxWrite) {
    z = frameBuffer_.readPixel(state.aux, x, y);
  }
  ditherAndWrite(x, y, pixel.r, pixel.g, pixel.b, pixel.a, z, writes);
}

void FbiPixelPipeline::ditherAndWrite(int x, int y, FxU32 r, FxU32 g, FxU32 b, FxU32 a, FxU32 z,
                                   const RenderState &state)
{
  const bool is565 = state.format == PixelFormat::Rgb565;
  const bool is1555 = state.format == PixelFormat::Argb1555;

  if (is565 || is1555) {
    if (state.dither != Dither::None) {
      // The 2x2 matrix is the centered top left of the 4x4 one.
      const int d = (state.dither == Dither::Matrix2x2) ? (kDitherMatrix[y & 1][x & 1] | 2)
                                                        : kDitherMatrix[y & 3][x & 3];
      r = dither5(r, d) << 3;
      g = is565 ? (dither6(g, d) << 2) : (dither5(g, d) << 3);
      b = dither5(b, d) << 3;
    } else {
      r &= 0xF8;
      g &= is565 ? 0xFC : 0xF8;
      b &= 0xF8;
    }
  }

  switch (state.format) {
  case PixelFormat::Argb8888:
    if (state.rgbWrite) {
      const FxU32 old = frameBuffer_.readPixel(state.color, x, y);
      const FxU32 color = (a << 24) | (r << 16) | (g << 8) | b;
      frameBuffer_.writePixel(state.color, x, y,
                              (old & ~state.colorWriteMask) | (color & state.colorWriteMask));
    }
    if (state.auxWrite || state.stencilWriteMask) frameBuffer_.writePixel(state.aux, x, y, z);
    break;

  case PixelFormat::Rgb565:
    if (state.rgbWrite) frameBuffer_.writePixel(state.color, x, y, (r << 8) | (g << 3) | (b >> 3));
    if (state.auxWrite) frameBuffer_.writePixel(state.aux, x, y, state.alphaBuffer ? a : (z >> 8));
    break;

  case PixelFormat::Argb1555:
    if (state.alpha1555 == Alpha1555::Zero) a = 0x00;
    if (state.alpha1555 == Alpha1555::One) a = 0xFF;
    a &= 0x80;
    if (state.rgbWrite) {
      frameBuffer_.writePixel(state.color, x, y, (a << 8) | (r << 7) | (g << 2) | (b >> 3));
    }
    if (state.auxWrite) frameBuffer_.writePixel(state.aux, x, y, state.alphaBuffer ? a : (z >> 8));
    break;

  case PixelFormat::Unsupported:
    context_.stats->unsimulated++;
    return;
  }

  increment(context_.sst().stats.fbiPixelsOut);
}

}  // namespace csim
