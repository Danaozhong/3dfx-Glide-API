#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "frame_buffer.hpp"

namespace csim {

namespace {

/**
 * @brief Moves @p value into [0, @p size).
 * @param[in,out] value The row or byte column, moved into the tile.
 * @param[in] size Rows or bytes of a tile.
 * @return How many whole @p size steps that took, rounded down, so negative
 * values borrow from the next tile.
 */
FxI32 carry(FxI32 &value, FxI32 size)
{
  FxI32 steps = value / size;
  if (value % size < 0) steps--;
  value -= steps * size;
  return steps;
}

/**
 * @brief Byte address of a pixel of a tiled buffer.
 *
 * The memory is a grid of 4 KB tiles, SST_TILE_WIDTH bytes by
 * SST_TILE_HEIGHT rows, with @p stride tiles per row of tiles; an address is
 * the tile number, the row in the tile and the byte column in the tile.
 *
 * @param[in] base Tiled address of pixel 0, 0.
 * @param[in] stride Tiles per row of tiles.
 * @param[in] depth Bytes per pixel.
 * @param[in] x Pixel column.
 * @param[in] y Pixel row.
 */
FxU32 tiledAddress(FxU32 base, FxU32 stride, FxU32 depth, int x, int y)
{
  base &= SST_BUFFER_BASE_ADDR;
  FxI32 tile = static_cast<FxI32>(base >> SST_BUFFER_BASE_T_SHIFT);
  FxI32 row = static_cast<FxI32>((base & SST_BUFFER_BASE_Y) >> SST_BUFFER_BASE_Y_SHIFT) + y;
  FxI32 column = static_cast<FxI32>(base & SST_BUFFER_BASE_X) + x * static_cast<FxI32>(depth);

  tile += carry(row, SST_TILE_HEIGHT) * static_cast<FxI32>(stride);
  tile += carry(column, SST_TILE_WIDTH);

  const FxU32 addr = (static_cast<FxU32>(tile) << SST_BUFFER_BASE_T_SHIFT) |
                     (static_cast<FxU32>(row) << SST_BUFFER_BASE_Y_SHIFT) |
                     static_cast<FxU32>(column);
  return addr & SST_BUFFER_BASE_ADDR;
}

/**
 * @brief 5 bits expanded to 8, with the high bits repeated.
 * @param[in] v The 5-bit channel.
 */
FxU32 expand5(FxU32 v) { return (v << 3) | (v >> 2); }
/**
 * @brief 6 bits expanded to 8, with the high bits repeated.
 * @param[in] v The 6-bit channel.
 */
FxU32 expand6(FxU32 v) { return (v << 2) | (v >> 4); }

}  // namespace

bool FrameBuffer::pixelAddress(const BufferLayout &buffer, int x, int y, FxU32 &addr) const
{
  if (buffer.tiled) {
    addr = tiledAddress(buffer.base, buffer.stride, buffer.depth, x, y);
  } else {
    addr = buffer.base + static_cast<FxU32>(y) * buffer.stride + static_cast<FxU32>(x) * buffer.depth;
  }
  return (addr % buffer.depth == 0) && (addr + buffer.depth <= context_.fbSize);
}

FxU32 FrameBuffer::readPixel(const BufferLayout &buffer, int x, int y) const
{
  FxU32 addr, data = 0;
  if (pixelAddress(buffer, x, y, addr)) {
    std::memcpy(&data, context_.frameBuffer() + addr, buffer.depth);
  }
  return data;
}

void FrameBuffer::writePixel(const BufferLayout &buffer, int x, int y, FxU32 data)
{
  FxU32 addr;
  if (pixelAddress(buffer, x, y, addr)) {
    std::memcpy(context_.frameBuffer() + addr, &data, buffer.depth);
  }
}

void FrameBuffer::storeBytes(FxU32 fbOffset, FxU32 data, FxU32 byteMask)
{
  for (FxU32 i = 0; i < 4; i++) {
    if ((byteMask & (1u << i)) || (fbOffset + i >= context_.fbSize)) continue;
    context_.frameBuffer()[fbOffset + i] = static_cast<FxU8>(data >> (8 * i));
  }
}

void FrameBuffer::captureImage(const BufferLayout &buffer, PixelFormat format, FxU32 width,
                               FxU32 height)
{
  imageWidth_ = width;
  imageHeight_ = height;
  image_.assign(static_cast<std::size_t>(width) * height, 0);

  for (FxU32 y = 0; y < height; y++) {
    for (FxU32 x = 0; x < width; x++) {
      const FxU32 p = readPixel(buffer, static_cast<int>(x), static_cast<int>(y));
      FxU32 argb;
      switch (format) {
      case PixelFormat::Argb8888:
        argb = p;
        break;
      case PixelFormat::Argb1555:
        argb = ((p & 0x8000) ? 0xFF000000 : 0) | (expand5((p >> 10) & 0x1F) << 16) |
               (expand5((p >> 5) & 0x1F) << 8) | expand5(p & 0x1F);
        break;
      default:
        argb = 0xFF000000 | (expand5((p >> 11) & 0x1F) << 16) |
               (expand6((p >> 5) & 0x3F) << 8) | expand5(p & 0x1F);
        break;
      }
      image_[static_cast<std::size_t>(y) * width + x] = argb;
    }
  }

  dumpImage();
  swapCount_++;
}

void FrameBuffer::dumpImage() const
{
  const char *prefix = std::getenv("CSIM_DUMP");
  if ((prefix == nullptr) || (*prefix == '\0')) return;

  const std::string name = prefix + std::to_string(swapCount_) + ".ppm";
  std::FILE *file = std::fopen(name.c_str(), "wb");
  if (file == nullptr) return;
  std::fprintf(file, "P6\n%u %u\n255\n", unsigned(imageWidth_), unsigned(imageHeight_));
  for (const unsigned int argb : image_) {
    const unsigned char rgb[3] = {static_cast<unsigned char>(argb >> 16),
                                  static_cast<unsigned char>(argb >> 8),
                                  static_cast<unsigned char>(argb)};
    std::fwrite(rgb, 1, sizeof(rgb), file);
  }
  std::fclose(file);
}

}  // namespace csim
