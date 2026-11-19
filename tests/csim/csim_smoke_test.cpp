// Smoke tests of the Glide3x library against the fake chip simulator
// (swlibs/csim).
//
// They go through the life cycle of a Glide application: initialize, open a
// window, draw frames, close and shut down. They fail if Glide reports an
// error, does not find the board, or the commands do not arrive in the
// command FIFO. What is drawn is not checked.

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

// On Windows, grSstWinOpen needs a window handle, like a real application
// passes. It is not shown, the fake board does not display anything.
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
