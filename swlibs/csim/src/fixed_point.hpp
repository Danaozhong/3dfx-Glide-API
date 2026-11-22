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

/**
 * @brief Brings the integer part of an iterator into [0, @p max], @p step
 * being its smallest increment.
 *
 * Out of range it saturates if @p clamp is set. Without clamping, one step
 * past either end still saturates, as the iteration can overshoot an edge of
 * the triangle by one; anything further wraps around.
 *
 * @param[in] value The integer part of the iterator.
 * @param[in] max The largest value, all ones.
 * @param[in] step The smallest increment.
 * @param[in] clamp Saturate instead of wrapping.
 */
constexpr FxU32 saturateOrWrap(FxI32 value, FxI32 max, FxI32 step, bool clamp)
{
  if (value >= 0 && value <= max) return static_cast<FxU32>(value);
  if (clamp || value == -step || value == max + step) {
    return (value < 0) ? 0 : static_cast<FxU32>(max);
  }
  return static_cast<FxU32>(value) & static_cast<FxU32>(max);
}

/**
 * @brief An 8-bit channel from a 12.12 color iterator.
 * @param[in] value The iterator.
 * @param[in] clamp Saturate instead of wrapping, see saturateOrWrap().
 */
constexpr FxU8 clampIterator(FxI32 value, bool clamp)
{
  return static_cast<FxU8>(saturateOrWrap(
      signExtend(static_cast<FxU32>(value) >> SST_RGBA_FRACBITS, 8 + kIteratorOverflowBits), 0xFF, 1,
      clamp));
}

}  // namespace csim

#endif  // CSIM_FIXED_POINT_HPP
