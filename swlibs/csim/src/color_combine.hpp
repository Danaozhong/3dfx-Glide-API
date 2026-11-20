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
 * @brief The color combine unit.
 * @param[in,out] local The local color; receives the combined color.
 * @param[in] other The other color.
 * @param[in] mselect7 The factor of SST_TC_MCMSELECT7.
 * @param[in] texture The texture color.
 * @param[in] config The mode bits.
 */
void combineColor(FxU8 local[4], const FxU8 other[4], const FxU8 mselect7[3],
                  const FxU8 texture[4], const CombineConfig &config);

/**
 * @brief The alpha combine unit.
 * @param[in,out] local The local color; its alpha receives the combined alpha.
 * @param[in] other The other color.
 * @param[in] iterated The iterated color.
 * @param[in] texture The texture color.
 * @param[in] config The mode bits.
 */
void combineAlpha(FxU8 local[4], const FxU8 other[4], const FxU8 iterated[4],
                  const FxU8 texture[4], const CombineConfig &config);

}  // namespace csim

#endif  // CSIM_COLOR_COMBINE_HPP
