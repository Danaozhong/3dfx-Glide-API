/**
 * @file
 * @brief A pixel of a triangle, from the rasterizer to the frame buffer
 * interface.
 */
#ifndef CSIM_FRAGMENT_HPP
#define CSIM_FRAGMENT_HPP

#include "glide.hpp"

namespace csim {

/**
 * @brief The texture coordinates of a TMU, iterated in 64 bits without
 * wrapping around.
 */
struct TexCoords {
  FxI64 s = 0;  ///< S/W, 14.18 (SST_ST_FRACBITS).
  FxI64 t = 0;  ///< T/W, 14.18.
  FxI64 w = 0;  ///< 1/W, 2.30 (SST_W_FRACBITS).
};

/// @brief A pixel of a triangle with its iterated parameters.
struct Fragment {
  int x = 0;  ///< Pixel column.
  int y = 0;  ///< Pixel row, before the y origin is applied.
  FxI32 r = 0;  ///< Red, 12.12 with SST_RGBA_SIZE bits.
  FxI32 g = 0;  ///< Green, 12.12 with SST_RGBA_SIZE bits.
  FxI32 b = 0;  ///< Blue, 12.12 with SST_RGBA_SIZE bits.
  FxI32 a = 0;  ///< Alpha, 12.12 with SST_RGBA_SIZE bits.
  FxI64 z64 = 0;  ///< Z, 4.24.20 in 32bpp and 4.16.28 in 16bpp.
  TexCoords tmu0;  ///< Texture coordinates of TMU0.
  FxU8 texture[4] = {};  ///< The color the TMUs deliver, r, g, b, a.
};

}  // namespace csim

#endif  // CSIM_FRAGMENT_HPP
