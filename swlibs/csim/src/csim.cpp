// The chip simulator and HAL functions of csim.h, csimstats.h and fxhal.h.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "board.hpp"
#include "display.hpp"

namespace csim {
namespace {

constexpr FxU32 kVendorId = 0x121a;  ///< 3dfx

/// @brief The simulated boards, and the HAL state Glide sees.
class Simulator {
public:
  /**
   * @brief A board, created on first use.
   * @param[in] index Index of the board.
   * @return nullptr if it cannot be created.
   */
  Board *board(FxU32 index);

  /**
   * @brief The board containing an address.
   * @param[in] addr An address of the program.
   * @param[out] offset Offset of @p addr in the board.
   * @return nullptr if no board contains @p addr.
   */
  Board *find(const volatile void *addr, FxU32 &offset);
  /**
   * @brief The board containing an address.
   * @param[in] addr An address of the program.
   * @return nullptr if no board contains @p addr.
   */
  Board *find(const volatile void *addr)
  {
    FxU32 offset;
    return find(addr, offset);
  }

  /**
   * @brief The index of a board.
   * @param[in] board One of the boards of the simulator.
   */
  FxU32 indexOf(const Board &board) const
  {
    return static_cast<FxU32>(&board - boards_.data());
  }

  /**
   * @brief A HAL read, see Board::specialRead.
   * @param[in] addr The address read.
   */
  template <typename T>
  T load(volatile void *addr);
  /**
   * @brief A HAL write, executed by the board of the address.
   * @param[in] addr The address written.
   * @param[in] data The value written.
   */
  template <typename T>
  void store(volatile void *addr, T data);

  /**
   * @brief A board if it was created.
   * @param[in] index Index of the board.
   * @return nullptr if the board was not created.
   */
  const Board *existing(FxU32 index) const
  {
    return ((index < boards_.size()) && boards_[index].exists()) ? &boards_[index] : nullptr;
  }

  HalInfo &halInfo() { return halInfo_; }
  CsimStats &stats() { return stats_; }
  /// @brief Flushes the CSIM_TRACE file.
  void flushTrace()
  {
    if (trace_) std::fflush(trace_.get());
  }

private:
  struct CloseFile {
    void operator()(std::FILE *file) const { std::fclose(file); }
  };

  std::array<Board, HAL_MAX_BOARDS> boards_;
  HalInfo halInfo_{};
  CsimStats stats_{};
  std::unique_ptr<std::FILE, CloseFile> trace_;
};

Board *Simulator::board(FxU32 index)
{
  if (index >= boards_.size()) return nullptr;

  Board &board = boards_[index];
  if (board.exists()) return &board;

  if (!board.create(stats_)) {
    std::fprintf(stderr, "csim: could not allocate the memory of board %u\n", unsigned(index));
    return nullptr;
  }

  if (!trace_) {
    if (const char *file = std::getenv("CSIM_TRACE")) trace_.reset(std::fopen(file, "w"));
  }

  // Glide overrides the PCI location (see _grSstDetectResources).
  FxDeviceInfo &info = halInfo_.boardInfo[index];
  info = FxDeviceInfo{};
  info.size = sizeof(info);
  info.virtAddr[0] = board.regs();
  info.virtAddr[1] = board.rawLfb();
  // There is no physical address, physAddr[] stays 0.
  info.vendorID = kVendorId;
  info.deviceID = SST_DEVICE_ID_AP_OEM;
  info.fbiRevision = 1;
  info.fbiVideoWidth = 640;
  info.fbiVideoHeight = 480;
  info.fbiMemSize = kFbiMemMB;
  info.numberTmus = kNumTmus;
  info.tmuMemSize[0] = kTmuMemMB;
  info.tmuMemSize[1] = kTmuMemMB;
  info.parentDevice = (index == 0);
  info.sstCSIM = board.regs();

  if (index >= halInfo_.boardsFound) halInfo_.boardsFound = index + 1;
  stats_.boardsMapped++;

  return &board;
}

Board *Simulator::find(const volatile void *addr, FxU32 &offset)
{
  for (Board &board : boards_) {
    if (const auto boardOffset = board.offsetOf(addr)) {
      offset = *boardOffset;
      return &board;
    }
  }
  return nullptr;
}

template <typename T>
T Simulator::load(volatile void *addr)
{
  FxU32 offset;
  if (Board *board = find(addr, offset)) {
    stats_.regReads++;
    if constexpr (sizeof(T) == 4) {
      if (const auto value = board->specialRead(offset)) return *value;
    }
  } else {
    stats_.foreignAccesses++;
  }
  return *static_cast<volatile T *>(addr);
}

template <typename T>
void Simulator::store(volatile void *addr, T data)
{
  *static_cast<volatile T *>(addr) = data;

  FxU32 offset;
  Board *board = find(addr, offset);
  if (board == nullptr) {
    stats_.foreignAccesses++;
    return;
  }

  if (trace_) {
    std::fprintf(trace_.get(), "W %u %08x %08x\n", unsigned(indexOf(*board)),
                 unsigned(offset), unsigned(data));
  }

  switch (board->written(offset, sizeof(T), data)) {
  case Write::Register:
    stats_.regWrites++;
    break;
  case Write::Fifo:
    stats_.fifoWords++;
    break;
  }
}

Simulator simulator;

/**
 * @brief FXTRUE if an address is in the memory of a board.
 * @param[in] addr An address of the program.
 */
FxBool isBoard(const volatile void *addr)
{
  return (simulator.find(addr) != nullptr) ? FXTRUE : FXFALSE;
}

}  // namespace
}  // namespace csim

using csim::Board;
using csim::isBoard;
using csim::simulator;

extern "C" {

/*---------------------------------------------------------------------------
** Tests, see csimstats.h
**-------------------------------------------------------------------------*/

void csimGetStats(CsimStats *stats)
{
  if (stats != nullptr) *stats = simulator.stats();
}

int csimGetSwappedImage(unsigned int board, CsimImage *image)
{
  const Board *b = simulator.existing(board);
  if ((b == nullptr) || (image == nullptr) || !b->chip().hasSwappedImage()) return 0;
  image->width = b->chip().imageWidth();
  image->height = b->chip().imageHeight();
  image->pixels = b->chip().imagePixels();
  return 1;
}

int csimWindowIsOpen(void)
{
  return csim::windowIsOpen() ? 1 : 0;
}

int csimIsBoardMemory(const void *p, unsigned int size)
{
  FxU32 offset;
  return (simulator.find(p, offset) != nullptr) && (size <= csim::kBoardSize - offset);
}

/*---------------------------------------------------------------------------
** Simulator, see csim.h and fxhal.h
**-------------------------------------------------------------------------*/

CsimPrivate *csimGetPrivate(const volatile void *sst)
{
  Board *board = simulator.find(sst);
  return (board != nullptr) ? &board->privateState() : nullptr;
}

volatile SstRegs *csimInit(FxU32 chipIndex)
{
  Board *board = simulator.board(chipIndex);
  return (board != nullptr) ? board->regs() : nullptr;
}

void csimMakeMultiFunctionDevice(volatile SstRegs *) {}

void FX_CALL csimInitDriver(FxU32, volatile FxU32 *, volatile FxU32 *) {}
void FX_CALL csimInitMemory(FxU32, volatile FxU32 *) {}
void FX_CALL csimInitHwAddress(volatile FxU32 *) {}

/*---------------------------------------------------------------------------
** Memory mapped register access
**-------------------------------------------------------------------------*/

void FX_CALL halStore8(volatile void *addr, FxU8 data)   { simulator.store(addr, data); }
void FX_CALL halStore16(volatile void *addr, FxU16 data) { simulator.store(addr, data); }
void FX_CALL halStore32(volatile void *addr, FxU32 data) { simulator.store(addr, data); }

void FX_CALL halStore32f(volatile void *addr, FxFloat data)
{
  FxU32 bits;
  std::memcpy(&bits, &data, sizeof(bits));
  simulator.store(addr, bits);
}

FxU8 FX_CALL halLoad8(volatile void *addr)   { return simulator.load<FxU8>(addr); }
FxU16 FX_CALL halLoad16(volatile void *addr) { return simulator.load<FxU16>(addr); }
FxU32 FX_CALL halLoad32(volatile void *addr) { return simulator.load<FxU32>(addr); }

/*---------------------------------------------------------------------------
** PCI configuration space and I/O ports. Not simulated.
**-------------------------------------------------------------------------*/

FxU32 FX_CALL halCfgLoad32(FxU16, int, FxU32, FxU32, FxU32, FxU32) { return 0; }
void FX_CALL halCfgStore32(FxU16, int, FxU32, FxU32, FxU32, FxU32, FxU32) {}

void  halOutPort8(FxU16, FxU8)   {}
void  halOutPort16(FxU16, FxU16) {}
void  halOutPort32(FxU16, FxU32) {}
FxU8  halInPort8(FxU16)          { return 0; }
FxU16 halInPort16(FxU16)         { return 0; }
FxU32 halInPort32(FxU16)         { return 0; }

/*---------------------------------------------------------------------------
** AGP memory. Not simulated, Glide uses the command FIFO in board memory.
**-------------------------------------------------------------------------*/

void FX_CALL agpMemInit(void) {}
FxU32 *FX_CALL agpMemAlloc(SstRegs *, unsigned) { return nullptr; }
void FX_CALL agpWriteMem32(FxU32 *, FxU32) {}
FxU32 FX_CALL agpReadMem32(FxU32 *) { return 0; }
void FX_CALL agpPhysWriteMem32(FxU32, FxU32, FxU32) {}
FxU32 FX_CALL agpPhysReadMem32(FxU32, FxU32) { return 0; }
FxU32 *FX_CALL agpPhysToVirt(FxU32, FxU32) { return nullptr; }

void FX_CALL agpVirtToPhys(FxU32 *, FxU32 *physAddrHi, FxU32 *physAddrLo)
{
  if (physAddrHi != nullptr) *physAddrHi = 0;
  if (physAddrLo != nullptr) *physAddrLo = 0;
}

/*---------------------------------------------------------------------------
** HAL (hardware abstraction layer)
**-------------------------------------------------------------------------*/

void FX_CALL fxHalPutenv(char *) {}

HalInfo *FX_CALL fxHalInit(FxU32)
{
  simulator.halInfo().csim = 1;
  simulator.halInfo().hw = 0;
  return &simulator.halInfo();
}

FxU32 FX_CALL fxHalNumBoardsInSystem(void) { return simulator.halInfo().boardsFound; }

SstRegs *FX_CALL fxHalMapBoard(FxU32 boardNum)
{
  Board *board = simulator.board(boardNum);
  return (board != nullptr) ? board->regs() : nullptr;
}

FxBool FX_CALL fxHalInitCmdFifo(SstRegs *sst, int which, FxU32 fifoStart, FxU32 size,
                                FxBool, FxBool disableHoles, FxBool agpEnable)
{
  Board *board = simulator.find(sst);
  if ((board == nullptr) || (which != 0) || agpEnable ||
      !board->initCmdFifo(fifoStart, size, disableHoles != FXFALSE)) {
    return FXFALSE;
  }
  simulator.stats().fifoInits++;
  return FXTRUE;
}

FxBool FX_CALL fxHalInitRegisters(SstRegs *sst)          { return isBoard(sst); }
FxBool FX_CALL fxHalInitRenderingRegisters(SstRegs *sst) { return isBoard(sst); }
FxBool FX_CALL fxHalInitGuiRegisters(SstGRegs *sstg)     { return isBoard(sstg); }
FxBool FX_CALL fxHalInitCmdAgpRegisters(SstCRegs *sstc)  { return isBoard(sstc); }
FxBool FX_CALL fxHalInitGamma(SstRegs *sst, FxFloat)     { return isBoard(sst); }
FxBool FX_CALL fxHalInitGammaRGB(SstRegs *sst, FxFloat, FxFloat, FxFloat) { return isBoard(sst); }
FxBool FX_CALL fxHalInitVideo(SstRegs *sst, FxU32, FxU32, FxVideoTimingInfo *) { return isBoard(sst); }

// The simulated chip is always idle and never shut down.
FxBool FX_CALL fxHalIdle(SstRegs *sst)       { return isBoard(sst); }
FxBool FX_CALL fxHalIdleNoNop(SstRegs *sst)  { return isBoard(sst); }
FxBool FX_CALL fxHalIdle2(SstRegs *sst)      { return isBoard(sst); }
FxBool FX_CALL fxHalIdleNoNop2(SstRegs *sst) { return isBoard(sst); }
FxBool FX_CALL fxHalInitUSWC(SstRegs *sst)   { return isBoard(sst); }
FxBool FX_CALL fxHalShutdown(SstRegs *sst)   { return isBoard(sst); }

void FX_CALL fxHalShutdownAll(void) { simulator.flushTrace(); }

FxBool FX_CALL fxHalGetDeviceInfo(SstRegs *sst, FxDeviceInfo *info)
{
  Board *board = simulator.find(sst);
  if ((board == nullptr) || (info == nullptr)) return FXFALSE;
  *info = simulator.halInfo().boardInfo[simulator.indexOf(*board)];
  return FXTRUE;
}

FxBool FX_CALL fxHalVsync(SstRegs *sst)    { return isBoard(sst); }
FxBool FX_CALL fxHalVsyncNot(SstRegs *sst) { return isBoard(sst); }

void FX_CALL fxHalInitVideoOverlaySurface(SstRegs *, FxU32, FxU32, FxU32, FxU32, FxU32,
                                          FxU32, FxU32, FxU32, FxU32, FxU32, FxU32,
                                          FxU32, FxU32)
{
}

}  // extern "C"
