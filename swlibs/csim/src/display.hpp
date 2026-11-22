/**
 * @file
 * @brief A window on the desktop that shows the swapped images, like the
 * monitor connected to the board.
 *
 * The window opens at the first swap. On Windows, it is a Win32 window with
 * a thread of its own, which forwards the keys typed into it to the active
 * window of the application (display_win32.cpp). Elsewhere it is a Qt
 * window, if the build found Qt (display_qt.cpp), which runs in the thread
 * that swaps: Qt wants its GUI in the main thread, on macOS it is a must. So
 * it only repaints when the application swaps. Without Qt, there is no
 * window (display_none.cpp). Setting the environment variable CSIM_WINDOW to
 * 0 disables it, e.g. for tests.
 */
#ifndef CSIM_DISPLAY_HPP
#define CSIM_DISPLAY_HPP

#include <cstdlib>
#include <cstring>

#include <3dfx.h>

namespace csim {

/**
 * @brief Shows an image in the window, opening it at the first call.
 * @param[in] pixels 0xAARRGGBB, top row first; copied.
 * @param[in] width Width of the image in pixels.
 * @param[in] height Height of the image in pixels.
 */
void showImage(const unsigned int *pixels, FxU32 width, FxU32 height);

/// @brief False if CSIM_WINDOW is 0.
inline bool windowEnabled()
{
  const char *value = std::getenv("CSIM_WINDOW");
  return (value == nullptr) || (std::strcmp(value, "0") != 0);
}

}  // namespace csim

#endif  // CSIM_DISPLAY_HPP
