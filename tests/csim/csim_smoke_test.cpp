// Smoke tests of the Glide3x library against the chip simulator
// (swlibs/csim).
//
// They go through the life cycle of a Glide application: initialize, open a
// window, draw frames, close and shut down. They fail if Glide reports an
// error, does not find the board, the commands do not arrive in the command
// FIFO, or the simulated chip renders something else than expected.
//
// Set CSIM_DUMP to a file name prefix to see the swapped images.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include <glide.h>
#include <csimstats.h>

#include "scene.hpp"

namespace {

#if defined(_MSC_VER) && defined(_DEBUG)
// Assertions and run-time check failures of the debug CRT open a message box,
// which nobody closes on a CI runner, so report them on stderr instead.
const bool crtReportsOnStderr = [] {
  for (int type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
    _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
  }
  return true;
}();
#endif

struct Vertex {
  float x, y;
  float r, g, b;
};

std::vector<std::string> glideErrors;

void errorCallback(const char *string, FxBool fatal)
{
  glideErrors.push_back(std::string(fatal ? "(fatal) " : "") + string);
}

CsimStats getStats()
{
  CsimStats stats;
  csimGetStats(&stats);
  return stats;
}

// The image of the last swap, 0xAARRGGBB.
class SwappedImage {
public:
  explicit SwappedImage(unsigned int board) { valid_ = csimGetSwappedImage(board, &image_) != 0; }

  bool valid() const { return valid_; }
  unsigned int width() const { return image_.width; }
  unsigned int height() const { return image_.height; }
  unsigned int at(unsigned int x, unsigned int y) const { return image_.pixels[y * width() + x]; }

  unsigned int count(unsigned int argb) const
  {
    unsigned int n = 0;
    for (unsigned int i = 0; i < width() * height(); i++) n += (image_.pixels[i] == argb);
    return n;
  }

private:
  CsimImage image_{};
  bool valid_ = false;
};

unsigned int red(unsigned int argb) { return (argb >> 16) & 0xFF; }
unsigned int green(unsigned int argb) { return (argb >> 8) & 0xFF; }
unsigned int blue(unsigned int argb) { return argb & 0xFF; }

// On Windows, grSstWinOpen needs a window handle, like a real application
// passes. It is not shown, the simulated board does not display anything.
// Elsewhere Glide takes 0.
class TestWindow {
public:
  TestWindow()
  {
#ifdef _WIN32
    window = CreateWindowExA(0, "STATIC", "Glide CSIM test", WS_POPUP, 0, 0, 640, 480,
                             NULL, NULL, GetModuleHandleA(NULL), NULL);
#endif
  }

  ~TestWindow()
  {
#ifdef _WIN32
    if (window != NULL) DestroyWindow(window);
#endif
  }

  TestWindow(const TestWindow &) = delete;
  TestWindow &operator=(const TestWindow &) = delete;

  // Glide takes window handles as FxU32, Windows keeps them in 32 bits for
  // that reason, also in 64-bit programs.
  FxU32 handle() const
  {
#ifdef _WIN32
    return static_cast<FxU32>(reinterpret_cast<uintptr_t>(window));
#else
    return 0;
#endif
  }

#ifdef _WIN32
  bool valid() const { return window != NULL; }
#else
  bool valid() const { return true; }
#endif

private:
#ifdef _WIN32
  HWND window = NULL;
#endif
};

// Glide is initialized for every test, and shut down afterwards.
class GlideCsim : public ::testing::Test {
protected:
  void SetUp() override
  {
    glideErrors.clear();
    grGlideInit();
    grErrorSetCallback(errorCallback);
  }

  void TearDown() override
  {
    if (context != 0) {
      EXPECT_TRUE(grSstWinClose(context));
    }
    grGlideShutdown();
    window.reset();

    for (const std::string &error : glideErrors) {
      ADD_FAILURE() << "Glide error: " << error;
    }
  }

  // Opens a 640x480 window with two color buffers and a depth buffer.
  void openWindow()
  {
    window.reset(new TestWindow);
    ASSERT_TRUE(window->valid()) << "could not create the window";

    grSstSelect(0);
    context = grSstWinOpen(window->handle(), GR_RESOLUTION_640x480, GR_REFRESH_60Hz,
                           GR_COLORFORMAT_ARGB, GR_ORIGIN_UPPER_LEFT, 2, 1);
    ASSERT_NE(context, static_cast<GrContext_t>(0)) << "grSstWinOpen failed";
  }

  // Sets up Gouraud shaded triangles.
  static void setupColoredTriangles()
  {
    grVertexLayout(GR_PARAM_XY,  0,                 GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_RGB, 2 * sizeof(float), GR_PARAM_ENABLE);
    grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                   GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE);
    grCullMode(GR_CULL_DISABLE);
  }

  static void drawFrame(int frame, int numTriangles)
  {
    grBufferClear(static_cast<GrColor_t>(frame), 0, 0xFFFF);

    for (int i = 0; i < numTriangles; i++) {
      const float x = static_cast<float>((i * 37 + frame) % 600);
      const float y = static_cast<float>((i * 53 + frame) % 440);
      const Vertex a = { x,         y,         255.0f, 0.0f,   0.0f   };
      const Vertex b = { x + 40.0f, y,         0.0f,   255.0f, 0.0f   };
      const Vertex c = { x,         y + 40.0f, 0.0f,   0.0f,   255.0f };
      grDrawTriangle(&a, &b, &c);
    }

    grBufferSwap(1);
  }

  GrContext_t context = 0;
  std::unique_ptr<TestWindow> window;
};

TEST_F(GlideCsim, FindsTheSimulatedBoard)
{
  FxI32 numBoards = 0;
  grGet(GR_NUM_BOARDS, sizeof(numBoards), &numBoards);
  EXPECT_EQ(numBoards, 1);
  EXPECT_GE(getStats().boardsMapped, 1u);
}

TEST_F(GlideCsim, OpensWindowAndSetsUpCommandFifo)
{
  const CsimStats before = getStats();

  ASSERT_NO_FATAL_FAILURE(openWindow());

  EXPECT_EQ(getStats().fifoInits, before.fifoInits + 1);
}

TEST_F(GlideCsim, DrawsFramesThroughTheCommandFifo)
{
  // Enough data for the 128kB command FIFO to wrap around several times.
  const int numFrames = 64;
  const int numTriangles = 256;

  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupColoredTriangles();

  const CsimStats before = getStats();
  for (int frame = 0; frame < numFrames; frame++) {
    drawFrame(frame, numTriangles);
  }
  grFinish();
  const CsimStats after = getStats();

  // Every triangle is at least a packet header and 3 * (x, y, r, g, b).
  EXPECT_GE(after.fifoWords - before.fifoWords, numFrames * numTriangles * 16u);
  EXPECT_GE(after.fifoWraps - before.fifoWraps, 1u);
  EXPECT_EQ(after.foreignAccesses, before.foreignAccesses)
      << "Glide accessed hardware outside of the simulated board";

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);
  EXPECT_EQ(after.swaps - before.swaps, static_cast<unsigned int>(numFrames));
  EXPECT_EQ(after.fastFills - before.fastFills, static_cast<unsigned int>(numFrames));
}

// The colors are exact in 16 and 32 bits per pixel.
TEST_F(GlideCsim, ClearsTheScreen)
{
  const GrColor_t color = 0xFF8400FF;

  ASSERT_NO_FATAL_FAILURE(openWindow());
  grDitherMode(GR_DITHER_DISABLE);
  grBufferClear(color, 0, 0xFFFF);
  grBufferSwap(1);
  grFinish();

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  ASSERT_EQ(image.width(), 640u);
  ASSERT_EQ(image.height(), 480u);
  EXPECT_EQ(image.count(color), 640u * 480u);
}

TEST_F(GlideCsim, DrawsGouraudShadedTriangles)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupColoredTriangles();
  grDitherMode(GR_DITHER_DISABLE);

  const CsimStats before = getStats();
  grBufferClear(0xFF000000, 0, 0xFFFF);
  // Flat red, the top left half of a 200 x 200 square.
  const Vertex flat[3] = {{100.0f, 100.0f, 255.0f, 0.0f, 0.0f},
                          {300.0f, 100.0f, 255.0f, 0.0f, 0.0f},
                          {100.0f, 300.0f, 255.0f, 0.0f, 0.0f}};
  grDrawTriangle(&flat[0], &flat[1], &flat[2]);
  // Red, green and blue corners.
  const Vertex shaded[3] = {{400.0f, 100.0f, 255.0f, 0.0f, 0.0f},
                            {600.0f, 100.0f, 0.0f, 255.0f, 0.0f},
                            {400.0f, 300.0f, 0.0f, 0.0f, 255.0f}};
  grDrawTriangle(&shaded[0], &shaded[1], &shaded[2]);
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  ASSERT_EQ(image.width(), 640u);
  ASSERT_EQ(image.height(), 480u);

  // The pixel centers of row y are inside for x < 299 - y + 100, so the
  // flat triangle has 199 + 198 + ... + 0 pixels, plus the ones on the
  // diagonal that the fill rule decides.
  const unsigned int redPixels = image.count(0xFFFF0000);
  EXPECT_GE(redPixels, 199u * 200u / 2u);
  EXPECT_LE(redPixels, 200u * 201u / 2u);
  EXPECT_EQ(image.at(100, 100), 0xFFFF0000u);
  EXPECT_EQ(image.at(297, 100), 0xFFFF0000u);
  EXPECT_EQ(image.at(100, 297), 0xFFFF0000u);
  EXPECT_EQ(image.at(99, 150), 0xFF000000u);
  EXPECT_EQ(image.at(150, 99), 0xFF000000u);
  EXPECT_EQ(image.at(250, 250), 0xFF000000u);

  // Near the corners, and where the colors are 1/2, 1/4 and 1/4.
  EXPECT_GE(red(image.at(400, 100)), 0xF0u);
  EXPECT_GE(green(image.at(597, 101)), 0xF0u);
  EXPECT_GE(blue(image.at(401, 297)), 0xF0u);
  const unsigned int mixed = image.at(450, 150);
  EXPECT_NEAR(red(mixed), 126.0, 8.0);
  EXPECT_NEAR(green(mixed), 64.0, 8.0);
  EXPECT_NEAR(blue(mixed), 64.0, 8.0);
}

// The scene of examples/csim_teapot: depth buffered triangle strips.
TEST_F(GlideCsim, DrawsTheUtahTeapot)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());

  const CsimStats before = getStats();
  teapot::Scene scene;
  scene.draw(30.0f, 640.0f, 480.0f);
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  ASSERT_EQ(image.width(), 640u);
  ASSERT_EQ(image.height(), 480u);

  // The red teapot covers about 41000 pixels, in front of the floor.
  unsigned int teapotPixels = 0;
  for (unsigned int y = 0; y < image.height(); y++) {
    for (unsigned int x = 0; x < image.width(); x++) {
      const unsigned int p = image.at(x, y);
      teapotPixels += (red(p) > 60) && (red(p) > 2 * green(p)) && (red(p) > 2 * blue(p));
    }
  }
  EXPECT_GT(teapotPixels, 35000u);
  EXPECT_LT(teapotPixels, 47000u);
  EXPECT_GT(red(image.at(300, 250)), 100u) << "teapot body";
  EXPECT_GT(blue(image.at(320, 2)), red(image.at(320, 2))) << "sky";
  EXPECT_GT(green(image.at(20, 470)), 80u) << "floor";
}

// The boards can be anywhere in the address space of 64-bit programs, so
// this fails if Glide passes board addresses through 32-bit variables.
TEST_F(GlideCsim, LocksTheFrameBufferInsideTheBoard)
{
  const FxU32 height = 480;
  const FxU32 pixel = 0x1234;

  ASSERT_NO_FATAL_FAILURE(openWindow());

  // Writes in the frame buffer format without the pixel pipeline go
  // directly into the color buffer.
  GrLfbInfo_t write;
  write.size = sizeof(write);
  ASSERT_TRUE(grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_ANY,
                        GR_ORIGIN_UPPER_LEFT, FXFALSE, &write));
  ASSERT_TRUE(csimIsBoardMemory(write.lfbPtr, write.strideInBytes * height))
      << "write pointer " << write.lfbPtr;
  const bool is32Bit = (write.writeMode == GR_LFBWRITEMODE_8888);
  if (is32Bit) {
    *static_cast<FxU32 *>(write.lfbPtr) = pixel;
  } else {
    *static_cast<FxU16 *>(write.lfbPtr) = static_cast<FxU16>(pixel);
  }
  EXPECT_TRUE(grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER));

  GrLfbInfo_t read;
  read.size = sizeof(read);
  ASSERT_TRUE(grLfbLock(GR_LFB_READ_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_ANY,
                        GR_ORIGIN_UPPER_LEFT, FXFALSE, &read));
  ASSERT_TRUE(csimIsBoardMemory(read.lfbPtr, read.strideInBytes * height))
      << "read pointer " << read.lfbPtr;
  if (is32Bit) {
    EXPECT_EQ(*static_cast<const FxU32 *>(read.lfbPtr), pixel);
  } else {
    EXPECT_EQ(*static_cast<const FxU16 *>(read.lfbPtr), static_cast<FxU16>(pixel));
  }
  EXPECT_TRUE(grLfbUnlock(GR_LFB_READ_ONLY, GR_BUFFER_BACKBUFFER));

  // Writes through the pixel pipeline go to the 3D LFB space instead.
  GrLfbInfo_t pipeline;
  pipeline.size = sizeof(pipeline);
  ASSERT_TRUE(grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_ANY,
                        GR_ORIGIN_UPPER_LEFT, FXTRUE, &pipeline));
  EXPECT_TRUE(csimIsBoardMemory(pipeline.lfbPtr, pipeline.strideInBytes * height))
      << "pixel pipeline pointer " << pipeline.lfbPtr;
  EXPECT_TRUE(grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER));
}

}  // namespace
