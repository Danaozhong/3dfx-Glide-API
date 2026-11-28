#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

#include <cstddef>
#include <initializer_list>

#include "board.hpp"

namespace csim {

namespace {

/// @brief Status of an idle chip: PCI FIFO free, not busy, no swaps pending.
constexpr FxU32 kIdleStatus = SST_PCIFIFO_FREE | SST_VRETRACE;

/// @brief Registers are aliased every 0x400 bytes in the 3D space, see SST_CHIP.
constexpr FxU32 k3dAliasMask = 0x3FF;
/// @brief The 3D registers of all chips.
constexpr FxU32 k3dRegisterSpace = 0x4000;

/**
 * @brief Allocates zeroed memory.
 * @param[in] size Bytes to allocate.
 * @return nullptr on failure.
 */
void *allocate(std::size_t size)
{
#if defined(_WIN64)
  // Above 4GB, so that the tests catch addresses truncated to 32 bits.
  return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);
#elif defined(_WIN32)
  return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
  void *p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  return (p == MAP_FAILED) ? nullptr : p;
#endif
}

/**
 * @brief Frees memory of allocate().
 * @param[in] p The memory.
 * @param[in] size The size passed to allocate().
 */
void release(void *p, std::size_t size)
{
#ifdef _WIN32
  (void)size;
  VirtualFree(p, 0, MEM_RELEASE);
#else
  munmap(p, size);
#endif
}

}  // namespace

Board::~Board()
{
  if (base_ != nullptr) release(base_, kBoardSize);
}

bool Board::create(CsimStats &stats)
{
  base_ = static_cast<FxU8 *>(allocate(kBoardSize));
  if (base_ == nullptr) return false;
  chip_.attach(base_, MBYTE(kFbiMemMB), stats);
  return true;
}

SstRegs *Board::regs() const
{
  return reinterpret_cast<SstRegs *>(base_ + SST_3D_OFFSET);
}

SstRegs *Board::rawLfb() const
{
  return reinterpret_cast<SstRegs *>(base_ + SST_RAW_LFB_OFFSET);
}

std::optional<FxU32> Board::offsetOf(const volatile void *addr) const
{
  const auto *p = static_cast<const volatile FxU8 *>(addr);
  if ((base_ == nullptr) || (p < base_) || (p >= base_ + kBoardSize)) return std::nullopt;
  return static_cast<FxU32>(p - base_);
}

bool Board::initCmdFifo(FxU32 fifoStart, FxU32 size, bool disableHoles)
{
  if ((size < 0x1000) || ((fifoStart & 0xFFF) != 0) || ((size & 0xFFF) != 0) ||
      (fifoStart + size > MBYTE(kFbiMemMB))) {
    return false;
  }

  fifoStart_ = fifoStart;
  fifoSize_ = size;
  chip_.initCmdFifo(fifoStart, size);

  FxU32 baseSize = ((size >> 12) - 1) | SST_EN_CMDFIFO;
  if (disableHoles) baseSize |= SST_CMDFIFO_DISABLE_HOLES;

  // Glide reads the registers through the board, and the CSIM private copy.
  auto *cRegs = reinterpret_cast<SstCRegs *>(base_ + SST_CMDAGP_OFFSET);
  for (SstCRegs *regs : {cRegs, &private_.cmd}) {
    regs->cmdFifo0.baseAddrL = fifoStart >> 12;
    regs->cmdFifo0.baseSize = baseSize;
    regs->cmdFifo0.readPtrL = fifoStart;
  }
  return true;
}

bool Board::isFifoWrite(FxU32 offset) const
{
  return (fifoSize_ != 0) &&
         (offset >= SST_RAW_LFB_OFFSET + fifoStart_) &&
         (offset < SST_RAW_LFB_OFFSET + fifoStart_ + fifoSize_);
}

Write Board::written(FxU32 offset, FxU32 size, FxU32 data)
{
  if (SST_IS_LFB_ADDR(offset)) {
    chip_.lfbWrite(offset - SST_LFB_OFFSET, data, size);
    return Write::Register;
  }
  if (size != 4) return Write::Register;

  if (isFifoWrite(offset)) {
    chip_.fifoWrite(offset - SST_RAW_LFB_OFFSET, data);
    return Write::Fifo;
  }
  if ((offset >= SST_3D_OFFSET) && (offset < SST_3D_OFFSET + k3dRegisterSpace)) {
    chip_.store3d(offset - SST_3D_OFFSET, data);
  }
  return Write::Register;
}

std::optional<FxU32> Board::specialRead(FxU32 offset) const
{
  if ((offset >= SST_3D_OFFSET) && (offset < SST_TEX_OFFSET) &&
      (((offset - SST_3D_OFFSET) & k3dAliasMask) == offsetof(SstRegs, status))) {
    return kIdleStatus;
  }

  if ((offset == SST_IO_OFFSET + offsetof(SstIORegs, status)) ||
      (offset == SST_2D_OFFSET + offsetof(SstGRegs, status))) {
    return kIdleStatus;
  }

  if (offset == SST_IO_OFFSET + offsetof(SstIORegs, vidCurrentLine)) {
    // There is no video timing, the beam moves one line per read, through
    // all values of the register, so that every wait for a line ends.
    return (currentLine_++ << SST_VIDEO_CURRENT_LINE_SHIFT) & SST_VIDEO_CURRENT_LINE;
  }

  if (offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.readPtrL)) {
    return chip_.fifoReadPtr();
  }

  if ((offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.readPtrH)) ||
      (offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.depth)) ||
      (offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.holeCount))) {
    return 0;
  }

  return std::nullopt;
}

}  // namespace csim
