/**
 * @file
 * @brief Command FIFO: decodes the packets that Glide writes into the FIFO
 * in the frame buffer.
 */
#ifndef CSIM_COMMAND_FIFO_HPP
#define CSIM_COMMAND_FIFO_HPP

#include "chip_context.hpp"

namespace csim {

/// @brief Where the packets of the command FIFO go.
class RegisterBus {
public:
  /**
   * @brief A 32-bit register write.
   * @param[in] offset Offset of the register in the address space of the board.
   * @param[in] data The value written.
   */
  virtual void storeRegister(FxU32 offset, FxU32 data) = 0;
  /**
   * @brief A write into the frame buffer.
   * @param[in] fbOffset Frame buffer offset of the word.
   * @param[in] data The word.
   * @param[in] byteMask Bit i disables byte i.
   */
  virtual void storeFrameBuffer(FxU32 fbOffset, FxU32 data, FxU32 byteMask) = 0;

protected:
  ~RegisterBus() = default;
};

/**
 * @brief The command FIFO of a chip.
 *
 * Glide writes packets into a ring buffer in the frame buffer, and the chip
 * executes them in order. Every packet starts with a header word whose low 3
 * bits are the packet type:
 *
 * | Packet | Purpose                     | Payload                                  |
 * |--------|-----------------------------|------------------------------------------|
 * | 0      | NOP and jumps               | None, the high address bits of JMP_AGP   |
 * | 1      | A block of registers        | n words, to consecutive registers        |
 * | 2      | 2D registers                | One word per bit set in a 29-bit mask    |
 * | 3      | Triangle strips and fans    | The parameters of n vertices             |
 * | 4      | Registers from a base on    | One word per bit set in a 14-bit mask    |
 * | 5      | Frame buffer or texture     | A start address, then n data words       |
 * | 6      | AGP transfer                | 4 words, not simulated                   |
 *
 * - Packet 0 jumps (JMP_LOCAL, JMP_AGP) wrap the ring buffer around. JSR
 *   and RET are not used by Glide.
 * - Packet 1 writes all words to the same register without SSTCP_INC.
 * - Packet 2 starts at clip0min, packet 4 at the register in its header.
 * - Packet 3 headers hold sSetupMode, the mask of the vertex parameters and
 *   the vertex count. Every vertex goes into the sV* registers and is
 *   followed by sBeginTriCMD or sDrawTriCMD, depending on the strip mode.
 * - Packet 5 headers hold active low byte enables for the first and the
 *   last data word, and the target: linear frame buffer, YUV, 3D LFB or
 *   texture port. Only the linear frame buffer is simulated.
 * - The headers of packets 3 and 4 also give a number of padding ("bogus")
 *   words after the payload, which the chip discards.
 */
class CommandFifo {
public:
  /**
   * @param[in] context The memory and statistics of the chip.
   * @param[in,out] bus Receives the register and frame buffer writes.
   */
  CommandFifo(const ChipContext &context, RegisterBus &bus) : context_(context), bus_(bus) {}

  /**
   * @brief Sets the FIFO up and resets the decoder.
   * @param[in] fifoStart Frame buffer offset of the FIFO.
   * @param[in] size Size of the FIFO in bytes.
   */
  void init(FxU32 fifoStart, FxU32 size);

  /**
   * @brief Executes a word written into the FIFO.
   *
   * The chip executes the words in the order of the FIFO, and Glide writes
   * them in that order. A word elsewhere is counted in CsimStats.fifoErrors.
   *
   * @param[in] fbOffset Frame buffer offset of the word.
   * @param[in] data The word.
   */
  void write(FxU32 fbOffset, FxU32 data);

  /// @brief Frame buffer offset of the next word the chip executes.
  FxU32 readPtr() const { return readPtr_; }

private:
  static constexpr int kNewPacket = -1;     ///< Decoder state: the next word is a header.
  static constexpr int kBogusDiscard = -2;  ///< Decoder state: discarding padding words.

  /**
   * @brief Decodes the next word of the FIFO.
   * @param[in] data The word.
   */
  void execute(FxU32 data);
  /**
   * @brief Packet 0: NOP and jumps.
   * @param[in] data The header, or the second word of JMP_AGP.
   * @param[in] secondWord True if @p data is the second word of JMP_AGP.
   */
  void packet0(FxU32 data, bool secondWord);
  /**
   * @brief Decodes the header of a packet.
   * @param[in] data The header.
   */
  void decodeHeader(FxU32 data);
  /**
   * @brief Packet 3 header: writes sSetupMode and lists the vertex registers
   * of the packet in vertexData_.
   * @param[in] data The header.
   */
  void packet3Header(FxU32 data);
  /**
   * @brief A payload word of packet 2 or 4: the next register in the mask.
   * @param[in] data The word.
   */
  void maskedWord(FxU32 data);
  /**
   * @brief A payload word of packet 3: the next vertex register, and the
   * triangle command after the last one of a vertex.
   * @param[in] data The word.
   */
  void vertexWord(FxU32 data);
  /**
   * @brief A payload word of packet 5: the start address, or a data word.
   * @param[in] data The word.
   */
  void frameBufferWord(FxU32 data);

  const ChipContext &context_;
  RegisterBus &bus_;
  FxU32 start_ = 0;    ///< Frame buffer offset of the FIFO.
  FxU32 end_ = 0;      ///< Frame buffer offset after the FIFO.
  FxU32 readPtr_ = 0;  ///< Frame buffer offset of the next word.

  /**
   * @name Decoder state.
   * @{
   */
  int state_ = kNewPacket;  ///< Packet type being decoded, or kNewPacket / kBogusDiscard.
  FxU32 packetWord_ = 0;    ///< Header of the packet.
  FxU32 registerPtr_ = 0;   ///< Board offset of the next register.
  FxU32 wordCount_ = 0;
  FxU32 bogusCount_ = 0;   ///< Padding words after the packet, discarded.
  FxU32 vertexCount_ = 0;
  FxU32 vertexLimit_ = 0;
  FxU32 vertexData_[16] = {};  ///< Packet 3 vertex registers, 0 terminated.
  FxU32 jmpAddr_ = 0;          ///< Packet 0 JMP_AGP, low address bits.
  /// @}
};

}  // namespace csim

#endif  // CSIM_COMMAND_FIFO_HPP
