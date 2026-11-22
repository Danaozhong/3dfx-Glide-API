/**
 * @file
 * @brief The rasterizer: walks the pixels of a triangle and iterates its
 * parameters.
 */
#ifndef CSIM_RASTERIZATION_HPP
#define CSIM_RASTERIZATION_HPP

#include "chip_context.hpp"
#include "fbi_pixel_pipeline.hpp"
#include "fragment.hpp"
#include "render_state.hpp"
#include "texture_unit.hpp"

namespace csim {

/**
 * @brief The 48-bit Z iterator.
 *
 * The registers hold only 32 bits of it, so the setup unit hands it to the
 * rasterizer directly.
 */
struct ZIterator {
  FxI64 start = 0;  ///< Z at vertex A.
  FxI64 dzdx = 0;   ///< Gradient in x.
  FxI64 dzdy = 0;   ///< Gradient in y.
};

/// @brief An edge of the triangle.
struct Edge {
  FxI32 dedx = 0;  ///< Gradient in x, 14.4.
  FxI32 dedy = 0;  ///< Gradient in y, 14.4.
  FxI32 e = 0;     ///< Error term, 28.4, at the center of pixel x0, y0.
  int x0 = 0;      ///< Pixel of the start vertex.
  int y0 = 0;      ///< Pixel of the start vertex.
};

/// @brief The rasterizer.
class Rasterizer {
public:
  /**
   * @param[in] context The registers and statistics of the chip.
   * @param[in,out] tmu0 Textures the pixels.
   * @param[in,out] fbiPixelPipeline Receives the pixels.
   */
  Rasterizer(const ChipContext &context, TextureUnit &tmu0, FbiPixelPipeline &fbiPixelPipeline)
      : context_(context), tmu0_(tmu0), fbiPixelPipeline_(fbiPixelPipeline)
  {
  }

  /**
   * @brief Draws the triangle in the vA, vB, vC and parameter registers
   * (sstTriangle).
   *
   * The spans go from vertex A up to B with the minor edge A to B, then up
   * to C with the minor edge B to C. With SST_PARMADJUST, the start values
   * are moved to the pixel center of vertex A, in the registers and in @p z.
   *
   * @param[in] state The decoded mode registers.
   * @param[in,out] z The Z iterator of the triangle.
   */
  void triangle(const RenderState &state, ZIterator &z);

  /**
   * @brief fastfillCMD (sstFastFill): fills the clip rectangle with color 1
   * and the depth of zaColor. Only the write masks and dithering apply.
   * @param[in] state The decoded mode registers.
   */
  void fastFill(const RenderState &state);

private:
  /**
   * @brief Loads the parameters at vertex A, moved to the center of its
   * pixel if SST_PARMADJUST is set, and sets up the edges (sst1Setup).
   * @param[in] state The decoded mode registers.
   * @param[in,out] z The Z iterator of the triangle.
   */
  void setupTriangle(const RenderState &state, ZIterator &z);
  /**
   * @brief Draws the pixels of a scanline inside of the major edge and
   * @p edge1 (sstSpan).
   * @param[in] y The scanline.
   * @param[in] edge1 The minor edge of the scanline.
   * @param[in] state The decoded mode registers.
   */
  void span(int y, const Edge &edge1, const RenderState &state);
  /**
   * @brief The clip rectangles (sstRectClip, without the stipple).
   * @param[in] x Pixel column.
   * @param[in] y Pixel row.
   * @param[in] state The decoded mode registers.
   * @return True if pixel @p x, @p y is drawn.
   */
  bool rectClip(int x, int y, const RenderState &state) const;

  const ChipContext &context_;
  TextureUnit &tmu0_;
  FbiPixelPipeline &fbiPixelPipeline_;

  Edge edge0_;      ///< Major edge, A to C.
  Edge edge1_;      ///< Minor edge, A to B; B to C is set up per triangle.
  Fragment start_;  ///< Parameters at the pixel of vertex A.
  FxI64 dzdx_ = 0, dzdy_ = 0;
  bool textureUnsimulated_ = false;  ///< The texturing of the triangle is not fully simulated.
};

}  // namespace csim

#endif  // CSIM_RASTERIZATION_HPP
