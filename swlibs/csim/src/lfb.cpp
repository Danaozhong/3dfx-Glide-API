#include "chip.hpp"

namespace csim {

namespace {

constexpr FxU32 kLfbWidth = 2048;  ///< Pixels per row of the 3D LFB, SST_LFB_ADDR_STRIDE.

/// @brief 5 bits expanded to 8, with the high bits repeated.
FxU32 expand5(FxU32 v) { return (v << 3) | (v >> 2); }
/// @brief 6 bits expanded to 8, with the high bits repeated.
FxU32 expand6(FxU32 v) { return (v << 2) | (v >> 4); }

/// @brief Bytes per pixel of an lfbMode format.
FxU32 pixelBytes(FxU32 format)
{
  switch (format) {
  case SST_LFB_565:
  case SST_LFB_555:
  case SST_LFB_1555:
  case SST_LFB_ZZ:
    return 2;
  default:
    return 4;
  }
}

/// @brief Exchanges red and blue of a 16-bit color with 5-bit red and blue.
FxU32 swapRedBlue16(FxU32 v, FxU32 format)
{
  if (format == SST_LFB_565) return (v & 0x07E0) | ((v & 0x1F) << 11) | ((v >> 11) & 0x1F);
  return (v & 0x83E0) | ((v & 0x1F) << 10) | ((v >> 10) & 0x1F);
}

/**
 * @brief Decodes a 16-bit color.
 * @param[in] v The color.
 * @param[in] format SST_LFB_565, SST_LFB_555 or SST_LFB_1555 (and the Z
 * variants).
 * @param[in] lanes SST_LFB_RGBALANES of lfbMode.
 * @param[out] pixel Receives the color.
 */
void decodeColor16(FxU32 v, FxU32 format, FxU32 lanes, LfbPixel &pixel)
{
  if ((lanes == SST_LFB_RGBALANES_ABGR) || (lanes == SST_LFB_RGBALANES_BGRA)) {
    v = swapRedBlue16(v, format);
  }
  pixel.hasColor = true;
  if (format == SST_LFB_565) {
    pixel.r = expand5((v >> 11) & 0x1F);
    pixel.g = expand6((v >> 5) & 0x3F);
    pixel.a = 0xFF;
  } else {
    pixel.r = expand5((v >> 10) & 0x1F);
    pixel.g = expand5((v >> 5) & 0x1F);
    pixel.a = ((format == SST_LFB_1555) && !(v & 0x8000)) ? 0x00 : 0xFF;
  }
  pixel.b = expand5(v & 0x1F);
}

/**
 * @brief Decodes a 32-bit color into 0xAARRGGBB.
 * @param[in] v The color.
 * @param[in] lanes SST_LFB_RGBALANES of lfbMode.
 */
FxU32 toArgb(FxU32 v, FxU32 lanes)
{
  switch (lanes) {
  case SST_LFB_RGBALANES_ABGR:
    return (v & 0xFF00FF00) | ((v & 0xFF) << 16) | ((v >> 16) & 0xFF);
  case SST_LFB_RGBALANES_RGBA:
    return (v >> 8) | (v << 24);
  case SST_LFB_RGBALANES_BGRA:
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24);
  default:
    return v;
  }
}

}  // namespace

void Chip::lfbWrite(FxU32 offset, FxU32 data, FxU32 size)
{
  const FxU32 mode = context_.sst().lfbMode;
  const FxU32 format = mode & SST_LFB_FORMAT;
  const FxU32 lanes = mode & SST_LFB_RGBALANES;
  const FxU32 bytes = pixelBytes(format);
  if ((size != 2) && (size != 4)) {
    context_.stats->unsimulated++;
    return;
  }
  if (size == 4) {
    if (mode & SST_LFB_WRITE_SWAP16) data = (data << 16) | (data >> 16);
    if (mode & SST_LFB_WRITE_BYTESWAP) {
      data = ((data & 0x00FF00FF) << 8) | ((data >> 8) & 0x00FF00FF);
    }
  } else if (mode & SST_LFB_WRITE_BYTESWAP) {
    data = ((data & 0xFF) << 8) | ((data >> 8) & 0xFF);
  }

  // Each pixel of a 16-bit format, or the one of a 32-bit format.
  FxU32 values[2] = {data & 0xFFFF, data >> 16};
  int count = static_cast<int>(size / bytes);
  if (bytes == 4) {
    if (size != 4) {
      context_.stats->unsimulated++;  // half of a 32-bit pixel
      return;
    }
    values[0] = data;
    count = 1;
  }

  const RenderState state = renderState();
  const FxU32 index = offset / bytes;
  for (int i = 0; i < count; i++) {
    const int x = static_cast<int>((index + static_cast<FxU32>(i)) % kLfbWidth);
    int y = static_cast<int>((index / kLfbWidth) % kLfbWidth);
    if (mode & SST_LFB_YORIGIN) y = state.yOrigin - y;

    const FxU32 v = values[i];
    LfbPixel pixel;
    switch (format) {
    case SST_LFB_565:
    case SST_LFB_555:
    case SST_LFB_1555:
      decodeColor16(v, format, lanes, pixel);
      break;
    case SST_LFB_888:
    case SST_LFB_8888: {
      const FxU32 argb = toArgb(v, lanes);
      pixel.hasColor = true;
      pixel.r = (argb >> 16) & 0xFF;
      pixel.g = (argb >> 8) & 0xFF;
      pixel.b = argb & 0xFF;
      pixel.a = (format == SST_LFB_8888) ? (argb >> 24) : 0xFF;
      break;
    }
    case SST_LFB_Z565:
    case SST_LFB_Z555:
    case SST_LFB_Z1555:
      decodeColor16(v & 0xFFFF, (format == SST_LFB_Z565)   ? SST_LFB_565
                                : (format == SST_LFB_Z555) ? SST_LFB_555
                                                           : SST_LFB_1555,
                    lanes, pixel);
      pixel.hasDepth = true;
      pixel.z = (v >> 16) << 8;
      break;
    case SST_LFB_ZZ:
      pixel.hasDepth = true;
      pixel.z = v << 8;
      break;
    case SST_LFB_Z32:
      pixel.hasDepth = true;
      pixel.z = v & 0xFFFFFF;
      break;
    default:
      context_.stats->unsimulated++;  // reserved formats
      return;
    }
    fbiPixelPipeline_.lfbWrite(x, y, pixel, (mode & SST_LFB_ENPIXPIPE) != 0, state);
  }
}

void Chip::store3dLfb(FxU32 offset, FxU32 data, FxU32 byteMask)
{
  switch (byteMask) {
  case 0x0:
    lfbWrite(offset, data, 4);
    break;
  case 0xC:
    lfbWrite(offset, data & 0xFFFF, 2);
    break;
  case 0x3:
    lfbWrite(offset + 2, data >> 16, 2);
    break;
  default:
    context_.stats->unsimulated++;  // single bytes
    break;
  }
}

}  // namespace csim
