// Renders the Utah teapot with Glide on the simulated Voodoo4/5 board of the
// HAL_CSIM build (swlibs/csim), and saves the swapped frames as PNG files.
//
//   csim_teapot [frames] [prefix]
//
// Writes <prefix>000.png, <prefix>001.png, ... (default 1 frame, "teapot"),
// with the teapot turning a full circle over the frames.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <glide.h>
#include <csimstats.h>

#include "scene.hpp"

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;

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

// An RGB PNG, with uncompressed deflate blocks.
bool writePng(const std::string &name, const CsimImage &image)
{
  std::vector<unsigned char> raw;
  for (unsigned int y = 0; y < image.height; y++) {
    raw.push_back(0);  // no filter
    for (unsigned int x = 0; x < image.width; x++) {
      const unsigned int argb = image.pixels[y * image.width + x];
      raw.push_back(static_cast<unsigned char>(argb >> 16));
      raw.push_back(static_cast<unsigned char>(argb >> 8));
      raw.push_back(static_cast<unsigned char>(argb));
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
  put32(header, image.width);
  put32(header, image.height);
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

}  // namespace

int main(int argc, char **argv)
{
  const int frames = (argc > 1) ? std::atoi(argv[1]) : 1;
  const std::string prefix = (argc > 2) ? argv[2] : "teapot";
  if (frames < 1) {
    std::fprintf(stderr, "usage: %s [frames] [prefix]\n", argv[0]);
    return 2;
  }

  FxU32 window = 0;
#ifdef _WIN32
  // grSstWinOpen needs a window on Windows, it is never shown.
  HWND hwnd = CreateWindowExA(0, "STATIC", "csim_teapot", WS_POPUP, 0, 0, kWidth, kHeight, NULL,
                              NULL, GetModuleHandleA(NULL), NULL);
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
  for (int frame = 0; frame < frames; frame++) {
    scene.draw(30.0f + 360.0f * static_cast<float>(frame) / static_cast<float>(frames),
               kWidth, kHeight);
    grBufferSwap(1);
    grFinish();

    CsimImage image;
    char number[16];
    std::snprintf(number, sizeof(number), "%03d", frame);
    const std::string name = prefix + number + ".png";
    if (!csimGetSwappedImage(0, &image) || !writePng(name, image)) {
      std::fprintf(stderr, "could not write %s\n", name.c_str());
      status = 1;
      break;
    }
    std::printf("%s\n", name.c_str());
  }

  CsimStats stats;
  csimGetStats(&stats);
  if (stats.unsimulated != 0) {
    std::fprintf(stderr, "warning: %u commands or pixels needed unsimulated features\n",
                 stats.unsimulated);
  }

  grSstWinClose(context);
  grGlideShutdown();
#ifdef _WIN32
  DestroyWindow(hwnd);
#endif
  return status;
}
