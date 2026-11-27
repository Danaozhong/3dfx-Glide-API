// Renders the Utah teapot with Glide3x on a Voodoo3/4/5, or on the simulated
// Voodoo4/5 board of the HAL_CSIM build (swlibs/csim).
//
//   teapot
//
// Turns the teapot until a key is pressed in the console (on Windows, also
// Esc), on the simulator also until its window is closed. On the simulator
// without a window (no Qt, or CSIM_WINDOW=0), writes teapot000.png instead.
//
//   teapot frames [prefix]
//
// Writes <prefix>000.png, <prefix>001.png, ... (default prefix "teapot"),
// with the teapot turning a full circle over the frames.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <conio.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <glide.h>
#ifdef TEAPOT_CSIM
#include <csimstats.h>
#endif

#include "scene.hpp"

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr float kStartAngle = 30.0f;
constexpr float kDegreesPerSecond = 60.0f;

std::uint32_t crc32(const unsigned char *data, std::size_t size, std::uint32_t crc = 0)
{
  crc = ~crc;
  for (std::size_t i = 0; i < size; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
  }
  return ~crc;
}

void put32(std::vector<unsigned char> &out, std::uint32_t v)
{
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<unsigned char>(v >> shift));
}

void chunk(std::vector<unsigned char> &out, const char *type, const std::vector<unsigned char> &data)
{
  put32(out, static_cast<std::uint32_t>(data.size()));
  std::vector<unsigned char> body(type, type + 4);
  body.insert(body.end(), data.begin(), data.end());
  out.insert(out.end(), body.begin(), body.end());
  put32(out, crc32(body.data(), body.size()));
}

// An RGB PNG of RGB565 pixels, with uncompressed deflate blocks.
bool writePng(const std::string &name, const std::vector<FxU16> &pixels)
{
  std::vector<unsigned char> raw;
  for (int y = 0; y < kHeight; y++) {
    raw.push_back(0);  // no filter
    for (int x = 0; x < kWidth; x++) {
      const unsigned int p = pixels[static_cast<std::size_t>(y) * kWidth + x];
      const unsigned int r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
      raw.push_back(static_cast<unsigned char>((r << 3) | (r >> 2)));
      raw.push_back(static_cast<unsigned char>((g << 2) | (g >> 4)));
      raw.push_back(static_cast<unsigned char>((b << 3) | (b >> 2)));
    }
  }

  std::vector<unsigned char> zlib = {0x78, 0x01};
  std::uint32_t a = 1, b = 0;
  for (unsigned char c : raw) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  for (std::size_t pos = 0; pos < raw.size() || pos == 0;) {
    const std::size_t len = std::min<std::size_t>(raw.size() - pos, 65535);
    const bool last = pos + len == raw.size();
    zlib.push_back(last ? 1 : 0);
    zlib.push_back(static_cast<unsigned char>(len));
    zlib.push_back(static_cast<unsigned char>(len >> 8));
    zlib.push_back(static_cast<unsigned char>(~len));
    zlib.push_back(static_cast<unsigned char>(~len >> 8));
    zlib.insert(zlib.end(), raw.begin() + pos, raw.begin() + pos + len);
    pos += len;
    if (last) break;
  }
  put32(zlib, (b << 16) | a);

  std::vector<unsigned char> header;
  put32(header, kWidth);
  put32(header, kHeight);
  header.insert(header.end(), {8, 2, 0, 0, 0});  // 8-bit RGB

  std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  chunk(png, "IHDR", header);
  chunk(png, "IDAT", zlib);
  chunk(png, "IEND", {});

  std::FILE *file = std::fopen(name.c_str(), "wb");
  if (file == nullptr) return false;
  const bool ok = std::fwrite(png.data(), 1, png.size(), file) == png.size();
  return (std::fclose(file) == 0) && ok;
}

// The keys typed into the console, without waiting for Enter.
class Keyboard {
public:
  Keyboard()
  {
#ifndef _WIN32
    if (isatty(STDIN_FILENO) && (tcgetattr(STDIN_FILENO, &saved_) == 0)) {
      termios unbuffered = saved_;
      unbuffered.c_lflag &= ~ICANON;
      tcsetattr(STDIN_FILENO, TCSANOW, &unbuffered);
      console_ = true;
    }
#endif
  }

  ~Keyboard()
  {
#ifndef _WIN32
    if (console_) tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
#endif
  }

  Keyboard(const Keyboard &) = delete;
  Keyboard &operator=(const Keyboard &) = delete;

  bool pressed()
  {
#ifdef _WIN32
    // On hardware, the window of the program has the focus, not the console.
    if (_kbhit()) {
      _getch();
      return true;
    }
    return (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
#else
    if (!console_) return false;
    fd_set keys;
    FD_ZERO(&keys);
    FD_SET(STDIN_FILENO, &keys);
    timeval noWait = {0, 0};
    if (select(STDIN_FILENO + 1, &keys, nullptr, nullptr, &noWait) <= 0) return false;
    char key;
    return read(STDIN_FILENO, &key, 1) == 1;
#endif
  }

private:
#ifndef _WIN32
  termios saved_ = {};
  bool console_ = false;
#endif
};

// False once the frames cannot be seen any more.
bool displayOpen()
{
#ifdef TEAPOT_CSIM
  return csimWindowIsOpen() != 0;
#else
  return true;  // the monitor of the board
#endif
}

void drawFrame(teapot::Scene &scene, float angle)
{
  scene.draw(angle, kWidth, kHeight);
  grBufferSwap(1);
  grFinish();
#ifdef _WIN32
  MSG msg;
  while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
#endif
}

// Writes the shown frame to <prefix><frame>.png.
bool saveFrame(const std::string &prefix, int frame)
{
  char number[16];
  std::snprintf(number, sizeof(number), "%03d", frame);
  const std::string name = prefix + number + ".png";

  // The color buffers of the window are RGB 565.
  std::vector<FxU16> pixels(static_cast<std::size_t>(kWidth) * kHeight);
  const bool ok = (grLfbReadRegion(GR_BUFFER_FRONTBUFFER, 0, 0, kWidth, kHeight,
                                   kWidth * sizeof(FxU16), pixels.data()) != FXFALSE) &&
                  writePng(name, pixels);
  if (!ok) {
    std::fprintf(stderr, "could not write %s\n", name.c_str());
    return false;
  }
  std::printf("%s\n", name.c_str());
  return true;
}

}  // namespace

int main(int argc, char **argv)
{
  const bool interactive = argc < 2;
  const int frames = interactive ? 1 : std::atoi(argv[1]);
  const std::string prefix = (argc > 2) ? argv[2] : "teapot";
  if (frames < 1) {
    std::fprintf(stderr, "usage: %s [frames [prefix]]\n", argv[0]);
    return 2;
  }

  FxU32 window = 0;
#ifdef _WIN32
  // grSstWinOpen needs a window on Windows. On the simulator, it is never
  // shown; on a Voodoo, it is the full screen window that has the focus.
#ifdef TEAPOT_CSIM
  const DWORD style = WS_POPUP;
#else
  const DWORD style = WS_POPUP | WS_VISIBLE;
#endif
  HWND hwnd = CreateWindowExA(0, "STATIC", "teapot", style, 0, 0, kWidth, kHeight, NULL, NULL,
                              GetModuleHandleA(NULL), NULL);
  window = static_cast<FxU32>(reinterpret_cast<std::uintptr_t>(hwnd));
#endif

  grGlideInit();
  grSstSelect(0);
  const GrContext_t context = grSstWinOpen(window, GR_RESOLUTION_640x480, GR_REFRESH_60Hz,
                                           GR_COLORFORMAT_ARGB, GR_ORIGIN_UPPER_LEFT, 2, 1);
  if (context == 0) {
    std::fprintf(stderr, "grSstWinOpen failed\n");
    grGlideShutdown();
    return 1;
  }

  teapot::Scene scene;
  int status = 0;
  drawFrame(scene, kStartAngle);
  if (interactive && displayOpen()) {
#ifdef TEAPOT_CSIM
    std::printf("Close the window or press a key to quit.\n");
#else
    std::printf("Press a key to quit.\n");
#endif
    std::fflush(stdout);
    Keyboard keyboard;
    const auto start = std::chrono::steady_clock::now();
    while (displayOpen() && !keyboard.pressed()) {
      const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - start;
      drawFrame(scene, kStartAngle + kDegreesPerSecond * elapsed.count());
    }
  } else {
    for (int frame = 0; frame < frames; frame++) {
      if (frame > 0) {
        drawFrame(scene, kStartAngle + 360.0f * static_cast<float>(frame) / static_cast<float>(frames));
      }
      if (!saveFrame(prefix, frame)) {
        status = 1;
        break;
      }
    }
  }

#ifdef TEAPOT_CSIM
  CsimStats stats;
  csimGetStats(&stats);
  if (stats.unsimulated != 0) {
    std::fprintf(stderr, "warning: %u commands or pixels needed unsimulated features\n",
                 stats.unsimulated);
  }
#endif

  grSstWinClose(context);
  grGlideShutdown();
#ifdef _WIN32
  DestroyWindow(hwnd);
#endif
  return status;
}
