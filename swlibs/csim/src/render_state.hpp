/**
 * @file
 * @brief The mode registers of the 3D engine, decoded once per triangle or
 * fastfill for the rasterizer and the FBI pixel pipeline.
 */
#ifndef CSIM_RENDER_STATE_HPP
#define CSIM_RENDER_STATE_HPP

#include "glide.hpp"

namespace csim {

/// @brief Pixel format of the color buffer.
enum class PixelFormat { Rgb565, Argb1555, Argb8888, Unsupported };

/// @brief Where pixel x, y of a color or aux buffer is, see FrameBuffer.
struct BufferLayout {
  FxU32 base = 0;      ///< Byte address in the frame buffer.
  FxU32 stride = 0;    ///< Bytes per row if linear, tiles per row if tiled.
  bool tiled = false;  ///< Tiled instead of linear memory.
  FxU32 depth = 2;     ///< Bytes per pixel.
};

/// @brief A clip rectangle. xMax and yMax are exclusive.
struct ClipRect {
  int xMin = 0, xMax = 0, yMin = 0, yMax = 0;

  /**
   * @brief True if a pixel is inside.
   * @param[in] x Pixel column.
   * @param[in] y Pixel row.
   */
  bool contains(int x, int y) const { return (x >= xMin) && (x < xMax) && (y >= yMin) && (y < yMax); }
};

/// @brief The less, equal and greater bits of a depth or alpha function.
struct CompareFunc {
  bool less = false, equal = false, greater = false;

  /**
   * @brief True if @p value compared to @p ref passes the function.
   * @param[in] value The new depth or alpha.
   * @param[in] ref The reference: the stored depth, or the alpha reference.
   */
  bool passes(FxU32 value, FxU32 ref) const
  {
    return (less && (value < ref)) || (equal && (value == ref)) || (greater && (value > ref));
  }
};

/// @brief Dithering of 15 and 16bpp writes.
enum class Dither { None, Matrix4x4, Matrix2x2 };

/// @brief What 15bpp writes as alpha bit.
enum class Alpha1555 { Source, Zero, One };

/// @brief Color inputs of the color combine unit.
enum class RgbSource { Iterated, Texture, Color0, Color1, IteratedAlpha, TextureAlpha, Color0Alpha,
                       Color1Alpha, Zero };
/// @brief Alpha inputs of the color combine unit.
enum class AlphaSource { Iterated, Texture, Color0, Color1, Z, W, Zero };

/// @brief Mode bits of the color combine unit (CCU), shared with the TMUs.
struct CombineConfig {
  FxU32 mode = 0;          ///< Combine function, SST_TC_* / SST_TCA_* layout.
  FxU32 rgbReverse = 0;    ///< 0 for the reverse blend, else 0xFF.
  FxU32 alphaReverse = 0;  ///< 0 for the reverse blend, else 0xFF.
  FxU32 combineMode = 0;   ///< Invert and shift bits, SST_CM_TC_* / SST_CM_TCA_* layout.
  FxU8 color1Alpha = 0;    ///< Alpha of color 1.
};

/// @brief The decoded mode registers for one triangle or fastfill.
struct RenderState {
  /**
   * @brief Decodes the mode registers.
   * @param[in] s The 3D registers.
   * @param[in] io The I/O registers, for the y origin in miscInit0.
   */
  static RenderState decode(const SstRegs &s, const SstIORegs &io);

  PixelFormat format = PixelFormat::Rgb565;  ///< Format of the color buffer.
  BufferLayout color;                        ///< The color buffer.
  BufferLayout aux;                          ///< The depth or alpha buffer.

  /**
   * @name Geometry
   * @{
   */
  bool flipY = false;  ///< Y origin at the bottom: y = yOrigin - y.
  int yOrigin = 0;     ///< Row of the bottom y origin.
  bool clip = false;   ///< Clip to clipRect.
  ClipRect clipRect;
  bool clip1 = false;           ///< Also clip to the second clip rectangle.
  bool clip1Exclusive = false;  ///< Draw outside of clip1Rect instead of inside.
  ClipRect clip1Rect;
  /// @}

  /**
   * @name Color path
   * @{
   */
  bool clampIterators = false;  ///< Clamp the color and depth iterators instead of wrapping.
  FxU32 color0 = 0;             ///< Constant color 0, 0xAARRGGBB.
  FxU32 color1 = 0;             ///< Constant color 1, 0xAARRGGBB.
  RgbSource otherRgb = RgbSource::Iterated;
  AlphaSource otherAlpha = AlphaSource::Iterated;
  RgbSource localRgb = RgbSource::Iterated;
  AlphaSource localAlpha = AlphaSource::Iterated;
  /// Local color is color 0 if bit 7 of the texture alpha is set, else iterated.
  bool localRgbFromTextureAlpha = false;
  RgbSource mselect7 = RgbSource::Iterated;  ///< The SST_CC_MCMSELECT7 factor.
  CombineConfig combine;
  /// @}

  /**
   * @name Tests
   * @{
   */
  bool alphaMask = false;
  bool chromaKey = false;
  FxU32 chromaKeyColor = 0;
  FxU32 chromaRange = 0;
  bool alphaTest = false;
  CompareFunc alphaFunc;
  FxU32 alphaRef = 0;
  bool depthTest = false;
  CompareFunc depthFunc;
  bool depthBias = false;
  FxI32 depthBiasValue = 0;       ///< In pixel depth units.
  bool depthFromZaColor = false;  ///< Compare zaColor instead of the pixel depth.
  FxU32 zaColor = 0;
  /// @}

  /**
   * @name Writes
   * @{
   */
  Dither dither = Dither::None;
  bool rgbWrite = false;
  bool auxWrite = false;     ///< Write depth or alpha.
  bool alphaBuffer = false;  ///< The aux buffer holds alpha instead of depth.
  FxU32 colorWriteMask = 0;  ///< Channels written in 32bpp.
  Alpha1555 alpha1555 = Alpha1555::Source;
  FxU32 stencilWriteMask = 0;
  FxU32 stencilRef = 0;
  /// @}

  /// Texture, fog, alpha blending, stencil test, W-buffer, stipple or AA.
  bool needsUnsimulated = false;
};

}  // namespace csim

#endif  // CSIM_RENDER_STATE_HPP
