/*
** Fake chip simulator (CSIM) and hardware abstraction layer (HAL) for Glide
** built with -DHAL_CSIM. See csim.h.
**
** Each simulated board is a block of memory laid out like the address space
** of a Voodoo4/5 (memBaseAddr0 followed by memBaseAddr1, see h3defs.h), and
** Glide accesses it through halLoad32/halStore32 and friends:
**  - writes are stored, reads return the stored value, so the board behaves
**    like memory;
**  - the status registers always report an idle chip with an empty FIFO;
**  - the command FIFO read pointer follows the writes, i.e. the FIFO is
**    consumed as soon as it is written.
** Nothing is executed or rendered.
**
** If CSIM_TRACE is set to a file name, all writes into board memory are
** logged there as "W <board> <offset> <value>".
*/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

#include <3dfx.h>
#include <fxdll.h>
#include <fxvid.h>  /* fxhal.h needs FxVideoTimingInfo and SstRegs declared before it */
#include <csim.h>
#include <fxhal.h>
#include <h3defs.h>
#include <h3gdefs.h>

#define CSIM_VENDOR_ID      0x121a
#define CSIM_FBI_MEM_MB     32
#define CSIM_TMU_MEM_MB     16
#define CSIM_NUM_TMUS       2

/* memBaseAddr0 and memBaseAddr1 */
#define CSIM_BOARD_SIZE     ((size_t)SST_MAX_LEGAL_OFFSET)

/* Status of an idle chip: PCI FIFO free, not busy, no swaps pending. */
#define CSIM_IDLE_STATUS    (SST_PCIFIFO_FREE | SST_VRETRACE)

/* Registers which are aliased every 0x400 bytes in the 3D space, see SST_CHIP. */
#define CSIM_3D_ALIAS_MASK  0x3FF

typedef struct {
  FxU8        *base;        /* start of the board address space        */
  CsimPrivate  priv;
  FxU32        fifoStart;   /* command FIFO, frame buffer offset        */
  FxU32        fifoSize;    /* command FIFO size in bytes, 0 = not set  */
  FxU32        fifoReadPtr; /* frame buffer offset after the last write */
} CsimBoard;

static CsimBoard  csimBoards[HAL_MAX_BOARDS];
static HalInfo    csimHalInfo;
static CsimStats  csimStats;
static FILE      *csimTrace;

/*---------------------------------------------------------------------------
** Boards
**-------------------------------------------------------------------------*/

static void *
csimAllocBoardMemory(size_t size)
{
#if defined(_WIN64)
  /* Above 4GB, so that the tests catch addresses truncated to 32 bits. */
  return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);
#elif defined(_WIN32)
  return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  return (p == MAP_FAILED) ? NULL : p;
#endif
}

static SstRegs *
csimBoardRegs(const CsimBoard *board)
{
  return (SstRegs *)(board->base + SST_3D_OFFSET);
}

/* Creates board 'index' if it does not exist yet. */
static CsimBoard *
csimGetBoard(FxU32 index)
{
  CsimBoard *board;
  FxDeviceInfo *info;

  if (index >= HAL_MAX_BOARDS) return NULL;

  board = &csimBoards[index];
  if (board->base != NULL) return board;

  board->base = (FxU8 *)csimAllocBoardMemory(CSIM_BOARD_SIZE);
  if (board->base == NULL) {
    fprintf(stderr, "csim: could not allocate the memory of board %u\n", (unsigned)index);
    return NULL;
  }

  if ((csimTrace == NULL) && (getenv("CSIM_TRACE") != NULL)) {
    csimTrace = fopen(getenv("CSIM_TRACE"), "w");
  }

  /* Glide overrides the PCI location (see _grSstDetectResources). */
  info = &csimHalInfo.boardInfo[index];
  memset(info, 0, sizeof(*info));
  info->size            = sizeof(*info);
  info->virtAddr[0]     = csimBoardRegs(board);
  info->virtAddr[1]     = (SstRegs *)(board->base + SST_RAW_LFB_OFFSET);
  /* There is no physical address, physAddr[] stays 0. */
  info->vendorID        = CSIM_VENDOR_ID;
  info->deviceID        = SST_DEVICE_ID_AP_OEM;
  info->fbiRevision     = 1;
  info->fbiVideoWidth   = 640;
  info->fbiVideoHeight  = 480;
  info->fbiMemSize      = CSIM_FBI_MEM_MB;
  info->numberTmus      = CSIM_NUM_TMUS;
  info->tmuMemSize[0]   = CSIM_TMU_MEM_MB;
  info->tmuMemSize[1]   = CSIM_TMU_MEM_MB;
  info->parentDevice    = (index == 0);
  info->sstCSIM         = csimBoardRegs(board);

  if (index >= csimHalInfo.boardsFound) csimHalInfo.boardsFound = index + 1;
  csimStats.boardsMapped++;

  return board;
}

/* Returns the board containing 'addr' and the offset in its address space. */
static CsimBoard *
csimFindBoard(const volatile void *addr, FxU32 *offset)
{
  const FxU8 *p = (const FxU8 *)addr;
  FxU32 i;

  for (i = 0; i < HAL_MAX_BOARDS; i++) {
    CsimBoard *board = &csimBoards[i];
    if ((board->base != NULL) && (p >= board->base) && (p < board->base + CSIM_BOARD_SIZE)) {
      *offset = (FxU32)(p - board->base);
      return board;
    }
  }
  return NULL;
}

static CsimBoard *
csimFindBoardByRegs(const volatile void *sst)
{
  FxU32 offset;
  return csimFindBoard(sst, &offset);
}

void
csimGetStats(CsimStats *stats)
{
  if (stats != NULL) *stats = csimStats;
}

int
csimIsBoardMemory(const void *p, unsigned int size)
{
  FxU32 offset;
  CsimBoard *board = csimFindBoard(p, &offset);

  return (board != NULL) && (size <= CSIM_BOARD_SIZE - offset);
}

/*---------------------------------------------------------------------------
** Simulator
**-------------------------------------------------------------------------*/

CsimPrivate *
csimGetPrivate(const volatile void *sst)
{
  CsimBoard *board = csimFindBoardByRegs(sst);
  return (board != NULL) ? &board->priv : NULL;
}

volatile SstRegs *
csimInit(FxU32 chipIndex)
{
  CsimBoard *board = csimGetBoard(chipIndex);
  return (board != NULL) ? csimBoardRegs(board) : NULL;
}

void
csimMakeMultiFunctionDevice(volatile SstRegs *sst)
{
  (void)sst;
}

void FX_CALL csimInitDriver(FxU32 numBytes, volatile FxU32 *memory, volatile FxU32 *hw)
{
  (void)numBytes; (void)memory; (void)hw;
}

void FX_CALL csimInitMemory(FxU32 numBytes, volatile FxU32 *memory)
{
  (void)numBytes; (void)memory;
}

void FX_CALL csimInitHwAddress(volatile FxU32 *hw)
{
  (void)hw;
}

/*---------------------------------------------------------------------------
** Memory mapped register access
**-------------------------------------------------------------------------*/

static FxBool
csimIsFifoWrite(const CsimBoard *board, FxU32 offset)
{
  return (board->fifoSize != 0) &&
         (offset >= SST_RAW_LFB_OFFSET + board->fifoStart) &&
         (offset <  SST_RAW_LFB_OFFSET + board->fifoStart + board->fifoSize);
}

/* Bookkeeping after 'size' bytes were written to 'offset' of 'board'. */
static void
csimWritten(CsimBoard *board, FxU32 offset, FxU32 value, FxU32 size)
{
  if (csimTrace != NULL) {
    fprintf(csimTrace, "W %u %08x %08x\n", (unsigned)(board - csimBoards),
            (unsigned)offset, (unsigned)value);
  }

  if ((size == 4) && csimIsFifoWrite(board, offset)) {
    const FxU32 fbOffset = offset - SST_RAW_LFB_OFFSET;

    csimStats.fifoWords++;
    /* Glide continues at the start of the FIFO after it wrote the jump
     * packet at the end. Within the FIFO it writes in increasing order. */
    if ((fbOffset == board->fifoStart) && (board->fifoReadPtr != board->fifoStart)) {
      csimStats.fifoWraps++;
      board->fifoReadPtr = fbOffset + 4;
    } else if (fbOffset + 4 > board->fifoReadPtr) {
      board->fifoReadPtr = fbOffset + 4;
    }
  } else {
    csimStats.regWrites++;
  }
}

/* Value of a read from 'offset', which does not simply return memory. */
static FxBool
csimSpecialRead(const CsimBoard *board, FxU32 offset, FxU32 *value)
{
  if ((offset >= SST_3D_OFFSET) && (offset < SST_TEX_OFFSET) &&
      (((offset - SST_3D_OFFSET) & CSIM_3D_ALIAS_MASK) == offsetof(SstRegs, status))) {
    *value = CSIM_IDLE_STATUS;
    return FXTRUE;
  }

  if ((offset == SST_IO_OFFSET + offsetof(SstIORegs, status)) ||
      (offset == SST_2D_OFFSET + offsetof(SstGRegs, status))) {
    *value = CSIM_IDLE_STATUS;
    return FXTRUE;
  }

  if (offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.readPtrL)) {
    *value = board->fifoReadPtr;
    return FXTRUE;
  }

  if ((offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.readPtrH)) ||
      (offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.depth)) ||
      (offset == SST_CMDAGP_OFFSET + offsetof(SstCRegs, cmdFifo0.holeCount))) {
    *value = 0;
    return FXTRUE;
  }

  return FXFALSE;
}

void FX_CALL
halStore8(volatile void *addr, FxU8 data)
{
  FxU32 offset;
  CsimBoard *board = csimFindBoard(addr, &offset);

  *(volatile FxU8 *)addr = data;
  if (board != NULL) csimWritten(board, offset, data, 1);
  else csimStats.foreignAccesses++;
}

void FX_CALL
halStore16(volatile void *addr, FxU16 data)
{
  FxU32 offset;
  CsimBoard *board = csimFindBoard(addr, &offset);

  *(volatile FxU16 *)addr = data;
  if (board != NULL) csimWritten(board, offset, data, 2);
  else csimStats.foreignAccesses++;
}

void FX_CALL
halStore32(volatile void *addr, FxU32 data)
{
  FxU32 offset;
  CsimBoard *board = csimFindBoard(addr, &offset);

  *(volatile FxU32 *)addr = data;
  if (board != NULL) csimWritten(board, offset, data, 4);
  else csimStats.foreignAccesses++;
}

void FX_CALL
halStore32f(volatile void *addr, FxFloat data)
{
  FxU32 bits;

  memcpy(&bits, &data, sizeof(bits));
  halStore32(addr, bits);
}

FxU8 FX_CALL
halLoad8(volatile void *addr)
{
  FxU32 offset;

  if (csimFindBoard(addr, &offset) != NULL) csimStats.regReads++;
  else csimStats.foreignAccesses++;
  return *(volatile FxU8 *)addr;
}

FxU16 FX_CALL
halLoad16(volatile void *addr)
{
  FxU32 offset;

  if (csimFindBoard(addr, &offset) != NULL) csimStats.regReads++;
  else csimStats.foreignAccesses++;
  return *(volatile FxU16 *)addr;
}

FxU32 FX_CALL
halLoad32(volatile void *addr)
{
  FxU32 offset, value;
  CsimBoard *board = csimFindBoard(addr, &offset);

  if (board == NULL) {
    csimStats.foreignAccesses++;
    return *(volatile FxU32 *)addr;
  }

  csimStats.regReads++;
  if (csimSpecialRead(board, offset, &value)) return value;
  return *(volatile FxU32 *)addr;
}

/*---------------------------------------------------------------------------
** PCI configuration space and I/O ports. Not simulated.
**-------------------------------------------------------------------------*/

FxU32 FX_CALL
halCfgLoad32(FxU16 port, int mechanism, FxU32 bus_number, FxU32 device_number,
             FxU32 function_number, FxU32 register_offset)
{
  (void)port; (void)mechanism; (void)bus_number; (void)device_number;
  (void)function_number; (void)register_offset;
  return 0;
}

void FX_CALL
halCfgStore32(FxU16 port, int mechanism, FxU32 bus_number, FxU32 device_number,
              FxU32 function_number, FxU32 register_offset, FxU32 data)
{
  (void)port; (void)mechanism; (void)bus_number; (void)device_number;
  (void)function_number; (void)register_offset; (void)data;
}

void  halOutPort8(FxU16 port, FxU8 data)    { (void)port; (void)data; }
void  halOutPort16(FxU16 port, FxU16 data)  { (void)port; (void)data; }
void  halOutPort32(FxU16 port, FxU32 data)  { (void)port; (void)data; }
FxU8  halInPort8(FxU16 port)                { (void)port; return 0; }
FxU16 halInPort16(FxU16 port)               { (void)port; return 0; }
FxU32 halInPort32(FxU16 port)               { (void)port; return 0; }

/*---------------------------------------------------------------------------
** AGP memory. Not simulated, Glide uses the command FIFO in board memory.
**-------------------------------------------------------------------------*/

void FX_CALL agpMemInit(void) {}

FxU32 * FX_CALL
agpMemAlloc(SstRegs *sst, unsigned sizeBytes)
{
  (void)sst; (void)sizeBytes;
  return NULL;
}

void  FX_CALL agpWriteMem32(FxU32 *virtAddr, FxU32 value) { (void)virtAddr; (void)value; }
FxU32 FX_CALL agpReadMem32(FxU32 *virtAddr)               { (void)virtAddr; return 0; }

void FX_CALL
agpPhysWriteMem32(FxU32 physAddrHi, FxU32 physAddrLo, FxU32 value)
{
  (void)physAddrHi; (void)physAddrLo; (void)value;
}

FxU32 FX_CALL
agpPhysReadMem32(FxU32 physAddrHi, FxU32 physAddrLo)
{
  (void)physAddrHi; (void)physAddrLo;
  return 0;
}

FxU32 * FX_CALL
agpPhysToVirt(FxU32 physAddrHi, FxU32 physAddrLo)
{
  (void)physAddrHi; (void)physAddrLo;
  return NULL;
}

void FX_CALL
agpVirtToPhys(FxU32 *virtAddr, FxU32 *physAddrHi, FxU32 *physAddrLo)
{
  (void)virtAddr;
  if (physAddrHi) *physAddrHi = 0;
  if (physAddrLo) *physAddrLo = 0;
}

/*---------------------------------------------------------------------------
** HAL (hardware abstraction layer)
**-------------------------------------------------------------------------*/

void FX_CALL fxHalPutenv(char *buf)  { (void)buf; }

HalInfo * FX_CALL
fxHalInit(FxU32 flags)
{
  (void)flags;
  csimHalInfo.csim = 1;
  csimHalInfo.hw   = 0;
  return &csimHalInfo;
}

FxU32 FX_CALL fxHalNumBoardsInSystem(void) { return csimHalInfo.boardsFound; }

SstRegs * FX_CALL
fxHalMapBoard(FxU32 boardNum)
{
  CsimBoard *board = csimGetBoard(boardNum);
  return (board != NULL) ? csimBoardRegs(board) : NULL;
}

FxBool FX_CALL
fxHalInitCmdFifo(SstRegs *sst, int which, FxU32 fifoStart, FxU32 size,
                 FxBool directExec, FxBool disableHoles, FxBool agpEnable)
{
  CsimBoard *board = csimFindBoardByRegs(sst);
  SstCRegs *cRegs;
  FxU32 baseSize;

  (void)directExec;
  if ((board == NULL) || (which != 0) || agpEnable || (size < 0x1000) ||
      ((fifoStart & 0xFFF) != 0) || ((size & 0xFFF) != 0) ||
      (fifoStart + size > MBYTE(CSIM_FBI_MEM_MB))) {
    return FXFALSE;
  }

  board->fifoStart   = fifoStart;
  board->fifoSize    = size;
  board->fifoReadPtr = fifoStart;

  baseSize = ((size >> 12) - 1) | SST_EN_CMDFIFO;
  if (disableHoles) baseSize |= SST_CMDFIFO_DISABLE_HOLES;

  cRegs = (SstCRegs *)(board->base + SST_CMDAGP_OFFSET);
  cRegs->cmdFifo0.baseAddrL = fifoStart >> 12;
  cRegs->cmdFifo0.baseSize  = baseSize;
  cRegs->cmdFifo0.readPtrL  = fifoStart;
  board->priv.cmd.cmdFifo0.baseAddrL = fifoStart >> 12;
  board->priv.cmd.cmdFifo0.baseSize  = baseSize;
  board->priv.cmd.cmdFifo0.readPtrL  = fifoStart;

  csimStats.fifoInits++;
  return FXTRUE;
}

FxBool FX_CALL fxHalInitRegisters(SstRegs *sst)          { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalInitRenderingRegisters(SstRegs *sst) { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalInitGuiRegisters(SstGRegs *sstg)     { return csimFindBoardByRegs(sstg) != NULL; }
FxBool FX_CALL fxHalInitCmdAgpRegisters(SstCRegs *sstc)  { return csimFindBoardByRegs(sstc) != NULL; }
FxBool FX_CALL fxHalInitGamma(SstRegs *sst, FxFloat gamma) { (void)gamma; return csimFindBoardByRegs(sst) != NULL; }

FxBool FX_CALL
fxHalInitGammaRGB(SstRegs *sst, FxFloat r, FxFloat g, FxFloat b)
{
  (void)r; (void)g; (void)b;
  return csimFindBoardByRegs(sst) != NULL;
}

FxBool FX_CALL
fxHalInitVideo(SstRegs *sst, FxU32 resolution, FxU32 refresh, FxVideoTimingInfo *timing)
{
  (void)resolution; (void)refresh; (void)timing;
  return csimFindBoardByRegs(sst) != NULL;
}

/* The fake chip is always idle and never shut down. */
FxBool FX_CALL fxHalIdle(SstRegs *sst)          { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalIdleNoNop(SstRegs *sst)     { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalIdle2(SstRegs *sst)         { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalIdleNoNop2(SstRegs *sst)    { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalInitUSWC(SstRegs *sst)      { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalShutdown(SstRegs *sst)      { return csimFindBoardByRegs(sst) != NULL; }

void FX_CALL
fxHalShutdownAll(void)
{
  if (csimTrace != NULL) fflush(csimTrace);
}

FxBool FX_CALL
fxHalGetDeviceInfo(SstRegs *sst, FxDeviceInfo *info)
{
  CsimBoard *board = csimFindBoardByRegs(sst);

  if ((board == NULL) || (info == NULL)) return FXFALSE;
  *info = csimHalInfo.boardInfo[board - csimBoards];
  return FXTRUE;
}

FxBool FX_CALL fxHalVsync(SstRegs *sst)         { return csimFindBoardByRegs(sst) != NULL; }
FxBool FX_CALL fxHalVsyncNot(SstRegs *sst)      { return csimFindBoardByRegs(sst) != NULL; }

void FX_CALL
fxHalInitVideoOverlaySurface(SstRegs *sst, FxU32 enable, FxU32 stereo,
                             FxU32 horizScaling, FxU32 dudx,
                             FxU32 verticalScaling, FxU32 dvdy,
                             FxU32 filterMode, FxU32 tiled, FxU32 pixFmt,
                             FxU32 clutBypass, FxU32 clutSelect,
                             FxU32 startAddress, FxU32 stride)
{
  (void)sst; (void)enable; (void)stereo; (void)horizScaling; (void)dudx;
  (void)verticalScaling; (void)dvdy; (void)filterMode; (void)tiled;
  (void)pixFmt; (void)clutBypass; (void)clutSelect; (void)startAddress;
  (void)stride;
}
