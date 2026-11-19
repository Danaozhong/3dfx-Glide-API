/*
** Statistics and checks of the fake CSIM board (see csim.h), for tests.
**
** This header does not depend on the Glide headers, so test programs can
** include it next to glide.h.
*/
#ifndef __CSIMSTATS_H__
#define __CSIMSTATS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* The fake board is linked into the Glide DLL, which has to export this. */
#if defined(_WIN32) && defined(CSIM_BUILD)
#define CSIM_API __declspec(dllexport)
#else
#define CSIM_API
#endif

typedef struct CsimStats_s {
  unsigned int boardsMapped;    /* boards created by csimInit/fxHalMapBoard   */
  unsigned int fifoInits;       /* calls to fxHalInitCmdFifo                  */
  unsigned int fifoWords;       /* 32-bit words written into a command FIFO  */
  unsigned int fifoWraps;       /* times the FIFO writes went back to start  */
  unsigned int regWrites;       /* other writes into a board address space   */
  unsigned int regReads;        /* reads from a board address space          */
  unsigned int foreignAccesses; /* hal accesses outside of all boards        */
} CsimStats;

/* Copies the statistics accumulated since the first board was created. */
CSIM_API void csimGetStats(CsimStats *stats);

/*
** Returns nonzero if the size bytes at p are memory of a board, e.g. a
** pointer that Glide hands out to the application.
*/
CSIM_API int csimIsBoardMemory(const void *p, unsigned int size);

#ifdef __cplusplus
}
#endif

#endif /* __CSIMSTATS_H__ */
