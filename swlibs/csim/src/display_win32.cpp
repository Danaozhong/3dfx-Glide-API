#include <windows.h>

#include <vector>

#include "display.hpp"

namespace csim {

namespace {

/// @brief Posted to the window to fit its client area to the image, wParam
/// by lParam pixels.
constexpr UINT kFitToImage = WM_APP + 1;
constexpr DWORD kStyle = WS_OVERLAPPEDWINDOW;
/// @brief MAPVK_VK_TO_CHAR, which older SDK headers do not define.
constexpr UINT kVirtualKeyToChar = 2;
const char kClassName[] = "CsimDisplay";

/// @brief The window and its image, shared by the window thread and the
/// thread that swaps. Win32 primitives only: MinGW with the win32 thread
/// model has no std::thread or std::mutex.
struct Window {
  CRITICAL_SECTION lock;            ///< Guards pixels, width and height.
  std::vector<unsigned int> pixels;
  FxU32 width = 0;
  FxU32 height = 0;
  HWND hwnd = nullptr;              ///< Set before created is signalled.
  HWND application = nullptr;       ///< Receives the keys typed into the window.
  HANDLE created = nullptr;
  volatile LONG closed = 0;
};

/// @brief Never freed: the window thread runs until the process ends, and a
/// DLL must not wait for its threads while it is unloaded.
Window *window = nullptr;
bool disabled = false;

/**
 * @brief Client area of @p width by @p height pixels.
 * @return The size of the whole window.
 */
SIZE windowSize(FxU32 width, FxU32 height)
{
  RECT rect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&rect, kStyle, FALSE);
  return {rect.right - rect.left, rect.bottom - rect.top};
}

void paint(HWND hwnd)
{
  PAINTSTRUCT ps;
  HDC dc = BeginPaint(hwnd, &ps);
  RECT client;
  GetClientRect(hwnd, &client);

  EnterCriticalSection(&window->lock);
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = static_cast<LONG>(window->width);
  info.bmiHeader.biHeight = -static_cast<LONG>(window->height);  // top row first
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  SetStretchBltMode(dc, COLORONCOLOR);
  StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0, static_cast<int>(window->width),
                static_cast<int>(window->height), window->pixels.data(), &info, DIB_RGB_COLORS,
                SRCCOPY);
  LeaveCriticalSection(&window->lock);

  EndPaint(hwnd, &ps);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
  case WM_PAINT:
    paint(hwnd);
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case kFitToImage: {
    const SIZE size = windowSize(static_cast<FxU32>(wParam), static_cast<FxU32>(lParam));
    SetWindowPos(hwnd, nullptr, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
  }
  // The application reads the keyboard from its own window. Keys that type
  // a character are forwarded as WM_CHAR only, because the application
  // translates WM_KEYDOWN into WM_CHAR again.
  case WM_KEYDOWN:
  case WM_KEYUP:
    if ((window->application != nullptr) && (MapVirtualKeyA(static_cast<UINT>(wParam), kVirtualKeyToChar) == 0)) {
      PostMessageA(window->application, message, wParam, lParam);
    }
    return 0;
  case WM_CHAR:
    if (window->application != nullptr) PostMessageA(window->application, message, wParam, lParam);
    return 0;
  case WM_DESTROY:
    InterlockedExchange(&window->closed, 1);
    PostQuitMessage(0);
    return 0;
  default:
    break;
  }
  return DefWindowProcA(hwnd, message, wParam, lParam);
}

DWORD WINAPI windowThread(LPVOID)
{
  HINSTANCE instance = GetModuleHandleA(nullptr);
  WNDCLASSA wc = {};
  wc.lpfnWndProc = windowProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = kClassName;
  RegisterClassA(&wc);

  const SIZE size = windowSize(window->width, window->height);
  window->hwnd = CreateWindowExA(0, kClassName, "CSIM - 3dfx Napalm", kStyle | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, size.cx, size.cy, nullptr, nullptr,
                                 instance, nullptr);
  if (window->hwnd == nullptr) InterlockedExchange(&window->closed, 1);
  SetEvent(window->created);
  if (window->hwnd == nullptr) return 0;

  MSG msg;
  while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  return 0;
}

/**
 * @brief Starts the window thread for images of @p width by @p height.
 * @return False if there is no window.
 */
bool open(FxU32 width, FxU32 height)
{
  window = new Window;
  InitializeCriticalSection(&window->lock);
  window->width = width;
  window->height = height;
  window->pixels.assign(static_cast<std::size_t>(width) * height, 0);
  // Glide falls back to the active window as well, if grSstWinOpen gets none.
  window->application = GetActiveWindow();
  window->created = CreateEventA(nullptr, TRUE, FALSE, nullptr);

  HANDLE thread = CreateThread(nullptr, 0, windowThread, nullptr, 0, nullptr);
  if (thread == nullptr) return false;
  CloseHandle(thread);
  WaitForSingleObject(window->created, INFINITE);
  return window->hwnd != nullptr;
}

}  // namespace

void showImage(const unsigned int *pixels, FxU32 width, FxU32 height)
{
  if (disabled || (width == 0) || (height == 0)) return;
  if (window == nullptr) {
    if (!windowEnabled() || !open(width, height)) {
      disabled = true;
      return;
    }
  }
  if (window->closed) return;

  EnterCriticalSection(&window->lock);
  const bool resized = (window->width != width) || (window->height != height);
  window->width = width;
  window->height = height;
  window->pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * height);
  LeaveCriticalSection(&window->lock);

  if (resized) PostMessageA(window->hwnd, kFitToImage, width, height);
  InvalidateRect(window->hwnd, nullptr, FALSE);
}

}  // namespace csim
