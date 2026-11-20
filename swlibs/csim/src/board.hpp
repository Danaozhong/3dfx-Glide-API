// A simulated Voodoo4/5 board, see csim.h.
#pragma once

#include <cstddef>
#include <optional>

#include "glide.hpp"

namespace csim {

// memBaseAddr0 followed by memBaseAddr1, see h3defs.h.
constexpr std::size_t kBoardSize = SST_MAX_LEGAL_OFFSET;

constexpr FxU32 kFbiMemMB = 32;
constexpr FxU32 kTmuMemMB = 16;
constexpr FxU32 kNumTmus = 2;

// What a write into the address space of a board was.
enum class Write {
  Register,  // anything outside of the command FIFO
  Fifo,      // a word of the command FIFO
  FifoWrap,  // a word at the start of the FIFO, after Glide wrapped around
};

// The address space of a board is plain memory: writes are stored and reads
// return the stored value, except for the registers in specialRead().
class Board {
public:
  Board() = default;
  ~Board();
  Board(const Board &) = delete;
  Board &operator=(const Board &) = delete;

  // Allocates the address space. Returns false if there is not enough memory.
  bool create();
  bool exists() const { return base_ != nullptr; }

  SstRegs *regs() const;
  SstRegs *rawLfb() const;
  CsimPrivate &privateState() { return private_; }

  // Offset of 'addr' in the address space, if it is inside.
  std::optional<FxU32> offsetOf(const volatile void *addr) const;

  // Sets up the command FIFO in the frame buffer. Returns false for
  // configurations a real chip does not support.
  bool initCmdFifo(FxU32 fifoStart, FxU32 size, bool disableHoles);

  // Bookkeeping after 'size' bytes were written at 'offset'.
  Write written(FxU32 offset, FxU32 size);

  // Value of a 32-bit read from 'offset', if it does not simply return
  // memory: the chip is always idle, and the command FIFO is consumed as
  // soon as it is written.
  std::optional<FxU32> specialRead(FxU32 offset) const;

private:
  bool isFifoWrite(FxU32 offset) const;

  FxU8 *base_ = nullptr;
  CsimPrivate private_{};
  FxU32 fifoStart_ = 0;    // command FIFO, frame buffer offset
  FxU32 fifoSize_ = 0;     // command FIFO size in bytes, 0 = not set up
  FxU32 fifoReadPtr_ = 0;  // frame buffer offset after the last write
};

}  // namespace csim
