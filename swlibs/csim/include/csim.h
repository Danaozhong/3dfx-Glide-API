/**
** @file
** @brief Interface of the 3dfx chip simulator (CSIM).
**
** The original simulator was never released. This header declares what the
** Glide sources need to compile with -DHAL_CSIM, and the C++ sources in src/
** implement it with a simulated board: the board address space is plain
** memory, register reads return what was written, the chip reports itself
** idle and executes the command FIFO as soon as it is written. The chip
** renders triangles (see src/chip.hpp), so Glide can be tested without
** hardware.
**
** Glide sets the simulated boards up through the fxHal* functions of
** fxhal.h and accesses their address space through halLoad32, halStore32
** and friends, which src/csim.cpp implements as well.
**
** Environment variables:
** - CSIM_TRACE: a file name; all writes into board memory are logged there
**   as "W <board> <offset> <value>".
** - CSIM_DUMP: a file name prefix; the swapped images are written to
**   "<CSIM_DUMP><n>.ppm", see src/frame_buffer.hpp.
** - CSIM_WINDOW: 0 disables the window that shows the swapped images, see
**   src/display.hpp.
**
*/
#ifndef __CSIM_H__
#define __CSIM_H__

#include <3dfx.h>
#include <h3regs.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The chip simulator state that the Glide API reads or writes. */
typedef struct CsimPrivate_s {
  struct {
    FxU32 chipCount;      /**< Number of chips on the simulated board. */
    FxU32 sliBandHeight;  /**< SLI band height in scanlines.           */
  } environment;
  SstCRegs cmd;           /**< Command FIFO registers of the chip.     */
} CsimPrivate;

/**
** @brief The private state of the chip with the registers @p sst.
**
** The original code kept this pointer in SstRegs.reservedD[1], which is only
** 32 bits wide. A function lookup works for 64-bit builds too.
**
** @param[in] sst The registers of the chip.
** @return NULL if @p sst is not a simulated chip.
*/
CsimPrivate *csimGetPrivate(const volatile void *sst);
/** @brief The private state of the chip with the registers @p sst. */
#define CSIM_PRIVATE(sst) csimGetPrivate(sst)

/**
** @brief Creates the simulated chip with the given index.
** @param[in] chipIndex Index of the chip.
** @return The registers of the chip or NULL on failure.
*/
volatile SstRegs *csimInit(FxU32 chipIndex);

/**
** @brief Makes the chip appear as a PCI multi-function device (SLI boards).
** @param[in] sst The registers of the chip.
*/
void csimMakeMultiFunctionDevice(volatile SstRegs *sst);

#include <csimstats.h>

#ifdef __cplusplus
}
#endif

#endif /* __CSIM_H__ */
