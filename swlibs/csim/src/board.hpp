/**
 * @file
 * @brief A simulated Voodoo4/5 board, see csim.h.
 */
#ifndef CSIM_BOARD_HPP
#define CSIM_BOARD_HPP

#include <cstddef>
#include <optional>

#include "chip.hpp"
#include "glide.hpp"

namespace csim {

/**
 * @brief Size of the address space of a board: memBaseAddr0 followed by
 * memBaseAddr1, see h3defs.h.
 */
constexpr std::size_t kBoardSize = SST_MAX_LEGAL_OFFSET;

constexpr FxU32 kFbiMemMB = 32;  ///< Frame buffer memory.
constexpr FxU32 kTmuMemMB = 16;  ///< Texture memory per TMU.

/// @brief What a write into the address space of a board was.
enum class Write {
  Register,  ///< A register write.
  Fifo,      ///< A word of the command FIFO.
};

/**
 * @brief A simulated VSA-100 (Napalm, Voodoo4/5) board.
 *
 * The address space of a board is plain memory. Writes are stored and reads
 * return the stored value, except for the registers in specialRead(). The
 * chip (chip.hpp) executes the command FIFO and 3D register writes.
 */
class Board {
public:
  Board() = default;
  ~Board();
  Board(const Board &) = delete;
  Board &operator=(const Board &) = delete;

  /**
   * @brief Allocates the address space.
   * @param[in,out] stats Statistics of the simulator, which the chip counts in.
   * @return False if there is not enough memory.
   */
  bool create(CsimStats &stats);
  /// @brief True after a successful create().
  bool exists() const { return base_ != nullptr; }

  /// @brief The 3D registers.
  SstRegs *regs() const;
  /// @brief The linear frame buffer.
  SstRegs *rawLfb() const;
  /// @brief The CSIM private state that Glide reads, see csim.h.
  CsimPrivate &privateState() { return private_; }
  const Chip &chip() const { return chip_; }

  /**
   * @brief Offset of @p addr in the address space, if it is inside.
   * @param[in] addr An address of the program.
   */
  std::optional<FxU32> offsetOf(const volatile void *addr) const;

  /**
   * @brief Sets up the command FIFO in the frame buffer.
   * @param[in] fifoStart Frame buffer offset of the FIFO.
   * @param[in] size Size of the FIFO in bytes.
   * @param[in] disableHoles Sets SST_CMDFIFO_DISABLE_HOLES in the FIFO registers.
   * @return False for configurations a real chip does not support.
   */
  bool initCmdFifo(FxU32 fifoStart, FxU32 size, bool disableHoles);

  /**
   * @brief Executes a write into the address space, after it was stored.
   * @param[in] offset Offset of the write in the address space.
   * @param[in] size Bytes written: 1, 2 or 4.
   * @param[in] data The value written.
   * @return What the write was.
   */
  Write written(FxU32 offset, FxU32 size, FxU32 data);

  /**
   * @brief Value of a 32-bit read, if it does not simply return memory: the
   * chip is always idle, the command FIFO is consumed as soon as it is
   * written, and the video beam (vidCurrentLine) moves with every read.
   * @param[in] offset Offset of the read in the address space.
   */
  std::optional<FxU32> specialRead(FxU32 offset) const;

private:
  /**
   * @brief True if @p offset is inside of the command FIFO.
   * @param[in] offset Offset in the address space.
   */
  bool isFifoWrite(FxU32 offset) const;

  FxU8 *base_ = nullptr;
  CsimPrivate private_{};
  Chip chip_;
  FxU32 fifoStart_ = 0;  ///< Command FIFO, frame buffer offset.
  FxU32 fifoSize_ = 0;   ///< Command FIFO size in bytes. 0 means not configured.
  mutable FxU32 currentLine_ = 0;  ///< Next value of vidCurrentLine.
};

}  // namespace csim

#endif  // CSIM_BOARD_HPP
