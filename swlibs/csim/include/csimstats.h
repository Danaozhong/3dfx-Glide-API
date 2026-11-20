/**
** @file
** @brief Statistics and checks of the simulated CSIM board for tests.
**
** This header does not depend on the Glide headers, so test programs can
** include it next to glide.h.
*/
#ifndef __CSIMSTATS_H__
#define __CSIMSTATS_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Export of the test functions: the simulated board is linked into the
** Glide DLL, which has to export them. */
#if defined(_WIN32) && defined(CSIM_BUILD)
#define CSIM_API __declspec(dllexport)
#else
#define CSIM_API
#endif

/** @brief Statistics accumulated since the first board was created. */
typedef struct CsimStats_s {
  unsigned int boardsMapped;    /**< Boards created by csimInit/fxHalMapBoard.  */
  unsigned int fifoInits;       /**< Calls to fxHalInitCmdFifo.                 */
  unsigned int fifoWords;       /**< 32-bit words written into a command FIFO. */
  unsigned int fifoWraps;       /**< Jump packets executed by the FIFO.         */
  unsigned int fifoErrors;      /**< Illegal packets, or words out of order.   */
  unsigned int regWrites;       /**< Other writes into a board address space.  */
  unsigned int regReads;        /**< Reads from a board address space.         */
  unsigned int foreignAccesses; /**< HAL accesses outside of all boards.       */
  unsigned int fastFills;       /**< fastfillCMD executed (buffer clears).     */
  unsigned int swaps;           /**< swapbufferCMD executed.                   */
  unsigned int unsimulated;     /**< Commands and pixels needing features the
                                     chip does not simulate, e.g. textures.   */
} CsimStats;

/**
** @brief Copies the statistics accumulated since the first board was created.
** @param[out] stats Receives the statistics. Ignored if NULL.
*/
CSIM_API void csimGetStats(CsimStats *stats);

/**
** @brief The image shown by the last swap of a board, top row first.
**
** Pixels are 0xAARRGGBB, with 5 and 6 bit channels expanded to 8 bits.
*/
typedef struct CsimImage_s {
  unsigned int width;
  unsigned int height;
  const unsigned int *pixels; /**< Not owned. Valid until the next swap of the board. */
} CsimImage;

/**
** @brief Gets the image shown by the last swap of @p board.
** @param[in] board Index of the board.
** @param[out] image Receives the image.
** @return Nonzero, with @p image filled, if the board has swapped at least once.
*/
CSIM_API int csimGetSwappedImage(unsigned int board, CsimImage *image);

/**
** @brief Checks that the @p size bytes at @p p are memory of a board, e.g. a
** pointer that Glide hands out to the application.
** @param[in] p The start of the memory.
** @param[in] size Bytes of the memory.
** @return Nonzero if they are.
*/
CSIM_API int csimIsBoardMemory(const void *p, unsigned int size);

#ifdef __cplusplus
}
#endif

#endif /* __CSIMSTATS_H__ */
