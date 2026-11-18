/*
** Do-nothing implementation of the chip simulator (CSIM) and hardware
** abstraction layer (HAL) interface that Glide uses when it is built with
** -DHAL_CSIM. See csim.h.
**
** There is no simulated hardware behind these functions: fxHalInit() reports
** that no boards are available, so grGlideInit() fails cleanly. Everything
** else only exists so that the library links.
*/
#include <stddef.h>

#include <3dfx.h>
#include <fxdll.h>
#include <fxvid.h>  /* fxhal.h needs FxVideoTimingInfo and SstRegs declared before it */
#include <csim.h>
#include <fxhal.h>

/*---------------------------------------------------------------------------
** Simulator
**-------------------------------------------------------------------------*/

CsimPrivate *
csimGetPrivate(const volatile void *sst)
{
  (void)sst;
  return NULL;
}

volatile SstRegs *
csimInit(FxU32 chipIndex)
{
  (void)chipIndex;
  return NULL;
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
** Memory mapped register access. Writes are dropped, reads return 0.
**-------------------------------------------------------------------------*/

void  FX_CALL halStore8(volatile void *addr, FxU8 data)    { (void)addr; (void)data; }
void  FX_CALL halStore16(volatile void *addr, FxU16 data)  { (void)addr; (void)data; }
void  FX_CALL halStore32(volatile void *addr, FxU32 data)  { (void)addr; (void)data; }
void  FX_CALL halStore32f(volatile void *addr, FxFloat data) { (void)addr; (void)data; }
FxU8  FX_CALL halLoad8(volatile void *addr)                { (void)addr; return 0; }
FxU16 FX_CALL halLoad16(volatile void *addr)               { (void)addr; return 0; }
FxU32 FX_CALL halLoad32(volatile void *addr)               { (void)addr; return 0; }

/*---------------------------------------------------------------------------
** PCI configuration space and I/O ports
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
** AGP memory
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
**
** No boards exist: fxHalInit() and fxHalMapBoard() fail, and all of the
** functions that need a board return failure.
**-------------------------------------------------------------------------*/

void       FX_CALL fxHalPutenv(char *buf)  { (void)buf; }
HalInfo *  FX_CALL fxHalInit(FxU32 flags)  { (void)flags; return NULL; }
FxU32      FX_CALL fxHalNumBoardsInSystem(void) { return 0; }
SstRegs *  FX_CALL fxHalMapBoard(FxU32 boardNum) { (void)boardNum; return NULL; }

FxBool FX_CALL
fxHalInitCmdFifo(SstRegs *sst, int which, FxU32 fifoStart, FxU32 size,
                 FxBool directExec, FxBool disableHoles, FxBool agpEnable)
{
  (void)sst; (void)which; (void)fifoStart; (void)size;
  (void)directExec; (void)disableHoles; (void)agpEnable;
  return FXFALSE;
}

FxBool FX_CALL fxHalInitRegisters(SstRegs *sst)            { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalInitRenderingRegisters(SstRegs *sst)   { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalInitGuiRegisters(SstGRegs *sstg)       { (void)sstg; return FXFALSE; }
FxBool FX_CALL fxHalInitCmdAgpRegisters(SstCRegs *sstc)    { (void)sstc; return FXFALSE; }
FxBool FX_CALL fxHalInitGamma(SstRegs *sst, FxFloat gamma) { (void)sst; (void)gamma; return FXFALSE; }

FxBool FX_CALL
fxHalInitGammaRGB(SstRegs *sst, FxFloat r, FxFloat g, FxFloat b)
{
  (void)sst; (void)r; (void)g; (void)b;
  return FXFALSE;
}

FxBool FX_CALL
fxHalInitVideo(SstRegs *sst, FxU32 resolution, FxU32 refresh, FxVideoTimingInfo *timing)
{
  (void)sst; (void)resolution; (void)refresh; (void)timing;
  return FXFALSE;
}

FxBool FX_CALL fxHalIdle(SstRegs *sst)          { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalIdleNoNop(SstRegs *sst)     { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalIdle2(SstRegs *sst)         { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalIdleNoNop2(SstRegs *sst)    { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalInitUSWC(SstRegs *sst)      { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalShutdown(SstRegs *sst)      { (void)sst; return FXFALSE; }
void   FX_CALL fxHalShutdownAll(void)           {}

FxBool FX_CALL
fxHalGetDeviceInfo(SstRegs *sst, FxDeviceInfo *info)
{
  (void)sst; (void)info;
  return FXFALSE;
}

FxBool FX_CALL fxHalVsync(SstRegs *sst)         { (void)sst; return FXFALSE; }
FxBool FX_CALL fxHalVsyncNot(SstRegs *sst)      { (void)sst; return FXFALSE; }

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
