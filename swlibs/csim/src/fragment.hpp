/**
 * @file
 * @brief A pixel of a triangle, from the rasterizer to the frame buffer
 * interface.
 */
#ifndef CSIM_FRAGMENT_HPP
#define CSIM_FRAGMENT_HPP

#include "glide.hpp"

namespace csim {

/// @brief A pixel of a triangle with its iterated parameters.
struct Fragment {
  int x = 0;  ///< Pixel column.
  int y = 0;  ///< Pixel row, before the y origin is applied.
  FxI32 r = 0;  ///< Red, 12.12 with SST_RGBA_SIZE bits.
  FxI32 g = 0;  ///< Green, 12.12 with SST_RGBA_SIZE bits.
  FxI32 b = 0;  ///< Blue, 12.12 with SST_RGBA_SIZE bits.
  FxI32 a = 0;  ///< Alpha, 12.12 with SST_RGBA_SIZE bits.
  FxI64 z64 = 0;  ///< Z, 4.24.20 in 32bpp and 4.16.28 in 16bpp.
};

}  // namespace csim

#endif  // CSIM_FRAGMENT_HPP
