/**
 * @file
 * @brief Texture mapping unit (TMU): texture coordinates to the filtered
 * texel, and the texture combine unit.
 *
 * The textures are in the frame buffer memory, where Glide downloads them
 * with linear frame buffer writes. Mipmap level L is at texBaseAddr plus
 * the sizes of all larger levels, counted from the level whose larger side
 * is 256 texels; L is the LOD, 0 for that level. The texture coordinates
 * are in texels of that level too, for both sides.
 */
#ifndef CSIM_TEXTURE_UNIT_HPP
#define CSIM_TEXTURE_UNIT_HPP

#include "chip_context.hpp"
#include "color_combine.hpp"
#include "fragment.hpp"

namespace csim {

/// @brief The mode registers of a TMU, decoded once per triangle.
struct TextureState {
  /**
   * @brief Decodes the registers of a TMU.
   * @param[in] tmu The registers of the TMU.
   */
  static TextureState decode(const SstRegs &tmu);

  /**
   * @name Coordinates
   * @{
   */
  bool perspective = false;  ///< Divide S and T by W.
  bool clampW = false;       ///< S and T are 0 where W is negative.
  bool clampS = false;
  bool clampT = false;
  bool mirrorS = false;
  bool mirrorT = false;
  /// @}

  /**
   * @name LOD, in 1/256 levels
   * @{
   */
  int lodMin = 0;   ///< The largest level of the texture.
  int lodMax = 0;   ///< The smallest level of the texture.
  int lodBias = 0;  ///< Added to the LOD.
  int aspect = 0;   ///< Log2 of the ratio of the sides.
  bool sIsWider = false;
  /// @}

  /**
   * @name Filters
   * @{
   */
  bool minBilinearRgb = false;
  bool magBilinearRgb = false;
  bool minBilinearAlpha = false;
  bool magBilinearAlpha = false;
  /// @}

  FxU32 format = 0;   ///< SST_TFORMAT, shifted down.
  FxU32 base = 0;     ///< Frame buffer address of LOD 0.
  bool ncc1 = false;  ///< Decode YIQ texels with nccTable1 instead of nccTable0.

  /**
   * @name Detail factor: (detailBias - LOD) << detailScale, at most detailMax
   * @{
   */
  int detailBias = 0;  ///< In 1/4 levels.
  int detailScale = 0;
  int detailMax = 0;
  /// @}

  /**
   * @name Texture combine
   * @{
   */
  CombineConfig combine;
  bool useCombineMode = false;  ///< Take the local, other and mselect7 inputs from combineMode.
  bool usesOther = true;        ///< The combine units use the color of the upstream TMU.
  /// @}

  /// Compressed, tiled, big (> 256) or multi-base textures, trilinear
  /// filtering, LOD dither or chroma key and range inputs.
  bool needsUnsimulated = false;
};

/// @brief A TMU.
class TextureUnit {
public:
  /**
   * @param[in] context The registers and memory of the chip.
   * @param[in] index The TMU, 0 to kNumTmus - 1.
   */
  TextureUnit(const ChipContext &context, int index) : context_(context), index_(index) {}

  /**
   * @brief Stores a palette entry, written into nccTable0[4] to nccTable0[11]
   * with bit 31 set.
   * @param[in] reg Index of the register in nccTable0.
   * @param[in] data The written value: bits 30..24 are bits 7..1 of the index,
   * bit 0 is that of @p reg; the low 24 bits are the entry.
   */
  void storePalette(int reg, FxU32 data);

  /// @brief Decodes the registers for the next triangle.
  void beginTriangle();

  /// @brief True if the triangle needs features that are not simulated.
  bool needsUnsimulated() const { return state_.needsUnsimulated; }
  /// @brief True if the texture combine uses the color of the upstream TMU.
  bool usesOther() const { return state_.usesOther; }

  /**
   * @brief Textures a pixel.
   * @param[in] coords The texture coordinates of the pixel.
   * @param[in] iterated The iterated color of the pixel.
   * @param[in] other The color of the upstream TMU.
   * @param[out] out The combined color.
   */
  void shade(const TexCoords &coords, const FxU8 iterated[4], const FxU8 other[4], FxU8 out[4]) const;

private:
  /// @brief A mipmap level.
  struct Level {
    FxU32 address = 0;  ///< Frame buffer address of texel 0, 0.
    int width = 1;
    int height = 1;
  };

  /**
   * @brief The address and size of a mipmap level.
   * @param[in] lod The level.
   */
  Level level(int lod) const;
  /**
   * @brief Reads and decodes a texel, applying the wrap, clamp or mirror
   * mode to its coordinates.
   * @param[in] level The mipmap level.
   * @param[in] s Column of the texel, outside of the level as well.
   * @param[in] t Row of the texel, outside of the level as well.
   * @param[out] out The texel as r, g, b, a.
   */
  void texel(const Level &level, FxI64 s, FxI64 t, FxU8 out[4]) const;
  /**
   * @brief Decodes a texel.
   * @param[in] raw The texel as stored.
   * @param[out] out The texel as r, g, b, a.
   */
  void decode(FxU32 raw, FxU8 out[4]) const;

  const ChipContext &context_;
  int index_;
  TextureState state_;
  FxU32 palette_[256] = {};  ///< As written, 24 bits.
  FxU32 ncc_[256] = {};      ///< The NCC table of the triangle, decoded to 0x00RRGGBB.
  double dsdx_ = 0, dtdx_ = 0, dwdx_ = 0;  ///< Gradients of S, T in texels and W.
  double dsdy_ = 0, dtdy_ = 0, dwdy_ = 0;
};

}  // namespace csim

#endif  // CSIM_TEXTURE_UNIT_HPP
