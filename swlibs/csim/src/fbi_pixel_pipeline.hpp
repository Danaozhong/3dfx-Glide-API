/**
 * @file
 * @brief The pixel pipeline of the frame buffer interface (FBI).
 *
 * Color selection, chroma key, color combine, alpha and depth test, dither
 * and the write through the write masks.
 */
#ifndef CSIM_FBI_PIXEL_PIPELINE_HPP
#define CSIM_FBI_PIXEL_PIPELINE_HPP

#include "chip_context.hpp"
#include "fragment.hpp"
#include "frame_buffer.hpp"
#include "render_state.hpp"

namespace csim {

/// @brief The pixel pipeline of the FBI, after the rasterizer and the TMUs.
class FbiPixelPipeline {
public:
  /**
   * @param[in] context The registers and statistics of the chip.
   * @param[in,out] frameBuffer Where the pixels are read and written.
   */
  FbiPixelPipeline(const ChipContext &context, FrameBuffer &frameBuffer)
      : context_(context), frameBuffer_(frameBuffer)
  {
  }

  /**
   * @brief Shades and writes a pixel of a triangle, after the clipping.
   * @param[in] fragment The pixel and its iterated parameters.
   * @param[in] state The decoded mode registers.
   */
  void shade(const Fragment &fragment, const RenderState &state);

  /**
   * @brief Writes a pixel of fastfillCMD: color 1 and zaColor, without the
   * tests.
   * @param[in] x Pixel column.
   * @param[in] y Pixel row, before the y origin is applied.
   * @param[in] state The decoded mode registers.
   */
  void fill(int x, int y, const RenderState &state);

private:
  /**
   * @brief The end of the pipeline: dither and write through the write
   * masks.
   * @param[in] x Pixel column.
   * @param[in] y Pixel row, after the y origin is applied.
   * @param[in] r Red, 8 bits.
   * @param[in] g Green, 8 bits.
   * @param[in] b Blue, 8 bits.
   * @param[in] a Alpha, 8 bits.
   * @param[in] z The pixel depth, in 32bpp with the stencil value on top.
   * @param[in] state The decoded mode registers.
   */
  void ditherAndWrite(int x, int y, FxU32 r, FxU32 g, FxU32 b, FxU32 a, FxU32 z,
                      const RenderState &state);

  const ChipContext &context_;
  FrameBuffer &frameBuffer_;
};

}  // namespace csim

#endif  // CSIM_FBI_PIXEL_PIPELINE_HPP
