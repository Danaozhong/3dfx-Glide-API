/**
 * @file
 * @brief The board memory that the units of a chip work on.
 */
#ifndef CSIM_CHIP_CONTEXT_HPP
#define CSIM_CHIP_CONTEXT_HPP

#include "glide.hpp"

namespace csim {

/// @brief The board memory and statistics, shared by the units of a chip.
struct ChipContext {
  FxU8 *base = nullptr;        ///< Address space of the board, see Board.
  FxU32 fbSize = 0;            ///< Bytes of frame buffer at SST_RAW_LFB_OFFSET.
  CsimStats *stats = nullptr;  ///< Statistics of the simulator.

  /// @brief The 3D registers.
  SstRegs &sst() const { return *reinterpret_cast<SstRegs *>(base + SST_3D_OFFSET); }
  /// @brief The I/O registers.
  SstIORegs &io() const { return *reinterpret_cast<SstIORegs *>(base + SST_IO_OFFSET); }
  /// @brief The first byte of the frame buffer.
  FxU8 *frameBuffer() const { return base + SST_RAW_LFB_OFFSET; }
};

}  // namespace csim

#endif  // CSIM_CHIP_CONTEXT_HPP
