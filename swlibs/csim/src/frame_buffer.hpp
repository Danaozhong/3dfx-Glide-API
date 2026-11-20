/**
 * @file
 * @brief The frame buffer memory of a chip: pixels of linear and tiled
 * buffers, and the images shown by swapbufferCMD.
 *
 * If the environment variable CSIM_DUMP is set, every swapped image is also
 * written to "<CSIM_DUMP><n>.ppm", n counting from 0.
 */
#ifndef CSIM_FRAME_BUFFER_HPP
#define CSIM_FRAME_BUFFER_HPP

#include <vector>

#include "chip_context.hpp"
#include "render_state.hpp"

namespace csim {

/// @brief The frame buffer memory of a chip.
class FrameBuffer {
public:
  /// @param[in] context The memory of the chip.
  explicit FrameBuffer(const ChipContext &context) : context_(context) {}

  /**
   * @brief Reads a pixel.
   * @param[in] buffer The color or aux buffer.
   * @param[in] x Pixel column.
   * @param[in] y Pixel row.
   * @return The pixel, 0 if it is outside of the memory.
   */
  FxU32 readPixel(const BufferLayout &buffer, int x, int y) const;
  /**
   * @brief Writes a pixel. Writes outside of the memory are dropped.
   * @param[in] buffer The color or aux buffer.
   * @param[in] x Pixel column.
   * @param[in] y Pixel row.
   * @param[in] data The pixel, in the low buffer.depth bytes.
   */
  void writePixel(const BufferLayout &buffer, int x, int y, FxU32 data);

  /**
   * @brief A write from the command FIFO.
   * @param[in] fbOffset Frame buffer offset of the word.
   * @param[in] data The word.
   * @param[in] byteMask Bit i disables byte i.
   */
  void storeBytes(FxU32 fbOffset, FxU32 data, FxU32 byteMask);

  /**
   * @brief Takes an image from a color buffer as the visible one,
   * converting it to 0xAARRGGBB.
   * @param[in] buffer The color buffer.
   * @param[in] format Pixel format of @p buffer.
   * @param[in] width Width of the image in pixels.
   * @param[in] height Height of the image in pixels.
   */
  void captureImage(const BufferLayout &buffer, PixelFormat format, FxU32 width, FxU32 height);

  /**
   * @name The image of the last captureImage, 0xAARRGGBB, top row first.
   * @{
   */
  bool hasImage() const { return imageWidth_ != 0; }
  FxU32 imageWidth() const { return imageWidth_; }
  FxU32 imageHeight() const { return imageHeight_; }
  const unsigned int *imagePixels() const { return image_.data(); }
  /// @}

private:
  /**
   * @brief Byte address of a pixel.
   * @param[in] buffer The color or aux buffer.
   * @param[in] x Pixel column.
   * @param[in] y Pixel row.
   * @param[out] addr Byte address in the frame buffer.
   * @return False if the pixel is outside of the memory or misaligned.
   */
  bool pixelAddress(const BufferLayout &buffer, int x, int y, FxU32 &addr) const;
  /// @brief Writes the image as PPM file if CSIM_DUMP is set.
  void dumpImage() const;

  const ChipContext &context_;
  FxU32 swapCount_ = 0;  ///< Images captured so far, numbers the dumps.
  FxU32 imageWidth_ = 0;
  FxU32 imageHeight_ = 0;
  std::vector<unsigned int> image_;  ///< The type of CsimImage::pixels, FxU32 is long on Windows.
};

}  // namespace csim

#endif  // CSIM_FRAME_BUFFER_HPP
