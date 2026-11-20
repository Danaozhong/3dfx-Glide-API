/**
 * @file
 * @brief Fixed point formats inside the chip.
 *
 * The formats of the registers are in h3defs.h (SST_XY_*, SST_RGBA_*, ...).
 */
#ifndef CSIM_FIXED_POINT_HPP
#define CSIM_FIXED_POINT_HPP

#include "glide.hpp"

namespace csim {

/**
 * @name Internal precisions of the rasterizer.
 * @{
 */

/// @brief Width of the Z iterator, which wraps around.
constexpr int kZIteratorBits = 48;
/// @brief Integer bits of the Z buffer.
constexpr int kDepthBits = 24;
/**
 * @brief Fraction bits of the Z iterator in 32bpp, 24.20.
 *
 * Shifted right by this, the iterator of both pixel formats gives the
 * kDepthBits pixel depth, in 16bpp with 8 fraction bits.
 */
constexpr int kZIteratorFracBits32Bpp = 20;
/// @brief Fraction bits of the Z iterator in 16bpp, 16.28.
constexpr int kZIteratorFracBits16Bpp = 28;
/**
 * @brief Bits above the 8-bit color and the kDepthBits depth that catch the
 * over- and underflow of the iterators.
 */
constexpr int kIteratorOverflowBits = 4;

/// @}

/**
 * @brief Sign extends the low @p bits bits of @p value.
 * @param[in] value The value, higher bits are ignored.
 * @param[in] bits Width of the value, 1 to 32.
 */
constexpr FxI32 signExtend(FxU32 value, int bits)
{
  return static_cast<FxI32>(value << (32 - bits)) >> (32 - bits);
}

/**
 * @brief Sign extends the low @p bits bits of @p value.
 * @param[in] value The value, higher bits are ignored.
 * @param[in] bits Width of the value, 1 to 64.
 */
constexpr FxI64 signExtend64(FxI64 value, int bits)
{
  return static_cast<FxI64>(static_cast<FxU64>(value) << (64 - bits)) >> (64 - bits);
}

/**
 * @brief A mask of the low @p n bits.
 * @param[in] n Width of the mask, 1 to 32.
 */
constexpr FxU32 mask(int n) { return 0xFFFFFFFFu >> (32 - n); }
/**
 * @brief A mask of the low @p n bits.
 * @param[in] n Width of the mask, 1 to 64.
 */
constexpr FxI64 mask64(int n) { return static_cast<FxI64>(~FxU64{0} >> (64 - n)); }

}  // namespace csim

#endif  // CSIM_FIXED_POINT_HPP
