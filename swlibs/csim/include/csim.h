/*
** Stub of the 3dfx chip simulator (CSIM) interface.
**
** The original simulator was never released. This header only declares
** what the Glide sources need to compile with -DHAL_CSIM, and
** src/csim_stub.c provides do-nothing implementations so the result links.
**
** A Glide library built against this stub contains all the simulator hooks
** but has no simulated hardware: it finds no boards and fails to initialize.
** To run against a real simulator, replace this directory with the real
** csim.h and library.
*/
#ifndef __CSIM_H__
#define __CSIM_H__

#include <3dfx.h>
#include <h3regs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-chip simulator state that Glide reads or writes. */
typedef struct CsimPrivate_s {
  struct {
    FxU32 chipCount;      /* number of chips on the simulated board */
    FxU32 sliBandHeight;  /* SLI band height in scanlines           */
  } environment;
  SstCRegs cmd;           /* command FIFO registers of the chip     */
} CsimPrivate;

/*
** The original code kept this pointer in SstRegs.reservedD[1], which is only
** 32 bits wide. A function lookup works for 64-bit builds too.
*/
CsimPrivate *csimGetPrivate(const volatile void *sst);
#define CSIM_PRIVATE(sst) csimGetPrivate(sst)

/* Create the simulated chip with the given index. Returns NULL on failure. */
volatile SstRegs *csimInit(FxU32 chipIndex);

/* Make the chip appear as a PCI multi-function device (SLI/AA boards). */
void csimMakeMultiFunctionDevice(volatile SstRegs *sst);

#ifdef __cplusplus
}
#endif

#endif /* __CSIM_H__ */
