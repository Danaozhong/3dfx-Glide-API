/**
 * @file
 * @brief The 3D part of a simulated Napalm chip, which renders Gouraud
 * shaded triangles into the frame buffer of a board (see board.hpp).
 *
 * The chip is a pipeline of units, in the layout of the 3dfx chip simulator
 * and with its fixed point formats (fixed_point.hpp):
 *
 * | Source                 | Unit                                                 |
 * |------------------------|------------------------------------------------------|
 * | command_fifo.cpp       | CommandFifo: packets to register writes              |
 * | registers.cpp          | Chip: register writes and commands                   |
 * | triangle_setup.cpp     | SetupUnit: float vertices to gradients               |
 * | rasterization.cpp      | Rasterizer: edge walking and iteration               |
 * | fbi_pixel_pipeline.cpp | FbiPixelPipeline: color select, tests, dither, write |
 * | color_combine.cpp      | color and alpha combine units                        |
 * | frame_buffer.cpp       | FrameBuffer: pixel addresses, swapped images         |
 *
 * The units get the mode registers decoded once per command, as a
 * RenderState (render_state.hpp), and pass pixels on as Fragments.
 *
 * Not simulated are the TMUs (textures), fog, alpha blending, stencil,
 * W-buffering, stipple, anti-aliasing and SLI. Pixels that would need any of
 * them are counted in CsimStats.unsimulated and drawn without it.
 */
#ifndef CSIM_CHIP_HPP
#define CSIM_CHIP_HPP

#include "chip_context.hpp"
#include "command_fifo.hpp"
#include "fbi_pixel_pipeline.hpp"
#include "frame_buffer.hpp"
#include "rasterization.hpp"
#include "triangle_setup.hpp"

namespace csim {

/// @brief sstGo command codes.
enum class Command {
  Nop,
  Triangle,
  FastFill,
  SwapBuffer,
  BeginTriangle,  ///< sBeginTriCMD, starts a strip or fan.
  DrawTriangle,   ///< sDrawTriCMD, adds a vertex and draws.
};

/// @brief The 3D part of a simulated Napalm chip.
class Chip final : private RegisterBus {
public:
  Chip() = default;
  Chip(const Chip &) = delete;
  Chip &operator=(const Chip &) = delete;

  /**
   * @brief Connects the chip to the memory of a board.
   * @param[in,out] base The address space of the board, see Board.
   * @param[in] fbSize Bytes of frame buffer at SST_RAW_LFB_OFFSET.
   * @param[in,out] stats Statistics of the simulator, which the chip counts in.
   */
  void attach(FxU8 *base, FxU32 fbSize, CsimStats &stats);

  /**
   * @brief Initializes the command FIFO.
   * @param[in] fifoStart Frame buffer offset of the FIFO.
   * @param[in] size Size of the FIFO in bytes.
   */
  void initCmdFifo(FxU32 fifoStart, FxU32 size) { commandFifo_.init(fifoStart, size); }

  /**
   * @brief Writes a word into the command FIFO.
   * @param[in] fbOffset Frame buffer offset of the word.
   * @param[in] data The word.
   */
  void fifoWrite(FxU32 fbOffset, FxU32 data) { commandFifo_.write(fbOffset, data); }

  /// @brief Returns the frame buffer offset of the next word the chip executes.
  FxU32 fifoReadPtr() const { return commandFifo_.readPtr(); }

  /**
   * @brief Writes a 32-bit 3D register.
   * @param[in] offset Offset from SST_3D_OFFSET, including the chip field.
   * @param[in] data The value written.
   */
  void store3d(FxU32 offset, FxU32 data);

  /**
   * @name The image shown by the last swapbufferCMD, 0xAARRGGBB
   * @{
   */
  bool hasSwappedImage() const { return frameBuffer_.hasImage(); }
  FxU32 imageWidth() const { return frameBuffer_.imageWidth(); }
  FxU32 imageHeight() const { return frameBuffer_.imageHeight(); }
  const unsigned int *imagePixels() const { return frameBuffer_.imagePixels(); }
  /// @}

private:
  /**
   * @name RegisterBus, for the command FIFO
   * @{
   */
  void storeRegister(FxU32 offset, FxU32 data) override;
  void storeFrameBuffer(FxU32 fbOffset, FxU32 data, FxU32 byteMask) override;
  /// @}

  /**
   * @brief Executes a command (sstGo).
   * @param[in] cmd The command.
   */
  void go(Command cmd);
  /// @brief The mode registers, decoded.
  RenderState renderState() const;

  /**
   * @brief Executes swapbufferCMD: shows leftOverlayBuf, and captures the
   * color buffer as the swapped image (see csimGetSwappedImage).
   */
  void swapBuffer();

  ChipContext context_;
  FrameBuffer frameBuffer_{context_};
  FbiPixelPipeline fbiPixelPipeline_{context_, frameBuffer_};
  Rasterizer rasterizer_{context_, fbiPixelPipeline_};
  SetupUnit setupUnit_{context_};
  CommandFifo commandFifo_{context_, *this};

  /// @brief Z of the last triangle set up, see ZIterator.
  ZIterator z_;
};

}  // namespace csim

#endif  // CSIM_CHIP_HPP
