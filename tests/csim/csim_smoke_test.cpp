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

#include <algorithm>
#include <cmath>
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

// The texture coordinates are divided by w, and in texels of the 256 texel
// side for both sides.
struct TexturedVertex {
  float x, y;
  float oow;
  float sow, tow;
};

// GR_TEXFMT_ARGB_8888 of g3ext.h.
const GrTextureFormat_t kTexFmtArgb8888 = 0x12;

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

// A color as it comes back from the 16bpp color buffer without dithering:
// truncated to 565, then expanded with the high bits repeated.
unsigned int as565(unsigned int argb)
{
  const unsigned int r = red(argb) >> 3, g = green(argb) >> 2, b = blue(argb) >> 3;
  return 0xFF000000 | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) |
         ((b << 3) | (b >> 2));
}

// The color of a 565 texel.
unsigned int fromRgb565(unsigned int texel)
{
  const unsigned int r = (texel >> 11) & 0x1F, g = (texel >> 5) & 0x3F, b = texel & 0x1F;
  return 0xFF000000 | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) |
         ((b << 3) | (b >> 2));
}

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

  // Sets up triangles that show the texture color of TMU0, point sampled and
  // wrapped, without dithering for exact colors.
  static void setupTexturedTriangles()
  {
    grVertexLayout(GR_PARAM_XY,  0,                 GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_Q,   2 * sizeof(float), GR_PARAM_ENABLE);
    grVertexLayout(GR_PARAM_ST0, 3 * sizeof(float), GR_PARAM_ENABLE);
    showTextureColor();
    grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                 GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
    grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_POINT_SAMPLED, GR_TEXTUREFILTER_POINT_SAMPLED);
    grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
    grTexMipMapMode(GR_TMU0, GR_MIPMAP_DISABLE, FXFALSE);
    grCullMode(GR_CULL_DISABLE);
    grDitherMode(GR_DITHER_DISABLE);
  }

  static void showTextureColor()
  {
    grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE,
                   GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
  }

  // White scaled by the texture alpha shows it as gray.
  static void showTextureAlpha()
  {
    grConstantColorValue(0xFFFFFFFF);
    grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_TEXTURE_ALPHA,
                   GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
  }

  // Downloads a texture with its mipmaps, largest first, into TMU0 and
  // selects it.
  static void loadTexture(GrLOD_t largeLod, GrLOD_t smallLod, GrTextureFormat_t format,
                          const void *data)
  {
    GrTexInfo info;
    info.smallLodLog2 = smallLod;
    info.largeLodLog2 = largeLod;
    info.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
    info.format = format;
    info.data = const_cast<void *>(data);
    const FxU32 address = grTexMinAddress(GR_TMU0);
    grTexDownloadMipMap(GR_TMU0, address, GR_MIPMAPLEVELMASK_BOTH, &info);
    grTexSource(GR_TMU0, address, GR_MIPMAPLEVELMASK_BOTH, &info);
  }

  // Draws a rectangle with the texture coordinates s0, t0 at the top left
  // and s1, t1 at the bottom right, and 1/w oowTop at the top and oowBottom
  // at the bottom.
  static void drawTexturedRect(float x0, float y0, float x1, float y1, float s0, float t0,
                               float s1, float t1, float oowTop = 1.0f, float oowBottom = 1.0f)
  {
    const TexturedVertex a = {x0, y0, oowTop, s0 * oowTop, t0 * oowTop};
    const TexturedVertex b = {x1, y0, oowTop, s1 * oowTop, t0 * oowTop};
    const TexturedVertex c = {x1, y1, oowBottom, s1 * oowBottom, t1 * oowBottom};
    const TexturedVertex d = {x0, y1, oowBottom, s0 * oowBottom, t1 * oowBottom};
    grDrawTriangle(&a, &b, &c);
    grDrawTriangle(&a, &c, &d);
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

// Programs wait for the vertical retrace with the video position, the beam
// has to move.
TEST_F(GlideCsim, ReportsTheMovingVideoPosition)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());

  FxI32 line = -1;
  ASSERT_EQ(grGet(GR_VIDEO_POSITION, sizeof(line), &line), sizeof(line));
  EXPECT_GE(line, 0);

  FxI32 position[2] = {-1, -1};
  ASSERT_EQ(grGet(GR_VIDEO_POSITION, sizeof(position), position), sizeof(position));
  EXPECT_NE(position[0], line);
  EXPECT_EQ(position[1], 0);
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

// The scene of examples/teapot: depth buffered triangle strips.
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

// A 565 texel with red growing along s, green along t.
FxU16 rampTexel(unsigned int i, unsigned int j)
{
  return static_cast<FxU16>(((i * 4) << 11) | ((j * 8) << 5) | (i + j + 8));
}

std::vector<FxU16> rampTexture()
{
  std::vector<FxU16> texture;
  for (unsigned int j = 0; j < 8; j++) {
    for (unsigned int i = 0; i < 8; i++) texture.push_back(rampTexel(i, j));
  }
  return texture;
}

// An 8 x 8 texture magnified to 128 x 128 pixels, 16 pixels per texel.
TEST_F(GlideCsim, MapsTexturesWithPointSampling)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupTexturedTriangles();
  const std::vector<FxU16> texture = rampTexture();

  const CsimStats before = getStats();
  grBufferClear(0xFF000000, 0, 0xFFFF);
  loadTexture(GR_LOD_LOG2_8, GR_LOD_LOG2_8, GR_TEXFMT_RGB_565, texture.data());
  drawTexturedRect(100.0f, 100.0f, 228.0f, 228.0f, 0.0f, 0.0f, 256.0f, 256.0f);
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  for (unsigned int j = 0; j < 8; j++) {
    for (unsigned int i = 0; i < 8; i++) {
      const unsigned int expected = fromRgb565(rampTexel(i, j));
      const unsigned int x = 100 + 16 * i, y = 100 + 16 * j;
      EXPECT_EQ(image.at(x, y), expected) << "texel " << i << ", " << j;
      EXPECT_EQ(image.at(x + 15, y + 15), expected) << "texel " << i << ", " << j;
    }
  }
  EXPECT_EQ(image.at(99, 150), 0xFF000000u);
  EXPECT_EQ(image.at(228, 150), 0xFF000000u);
}

// Twice the texture across: repeated, clamped to its last column, or mirrored.
TEST_F(GlideCsim, WrapsClampsAndMirrorsTextureCoordinates)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupTexturedTriangles();
  const std::vector<FxU16> texture = rampTexture();
  const GrTextureClampMode_t modes[3] = {GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_CLAMP,
                                         GR_TEXTURECLAMP_MIRROR_EXT};

  const CsimStats before = getStats();
  grBufferClear(0xFF000000, 0, 0xFFFF);
  loadTexture(GR_LOD_LOG2_8, GR_LOD_LOG2_8, GR_TEXFMT_RGB_565, texture.data());
  for (int m = 0; m < 3; m++) {
    grTexClampMode(GR_TMU0, modes[m], modes[m]);
    const float y = 20.0f + 150.0f * static_cast<float>(m);
    drawTexturedRect(100.0f, y, 356.0f, y + 128.0f, 0.0f, 0.0f, 512.0f, 256.0f);
  }
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  const unsigned int j = 3;
  for (unsigned int k = 0; k < 16; k++) {
    const unsigned int wrapped = k % 8;
    const unsigned int clamped = (k < 8) ? k : 7;
    const unsigned int mirrored = (k < 8) ? k : 15 - k;
    const unsigned int x = 100 + 16 * k + 8;
    const unsigned int y = 16 * j + 8;
    EXPECT_EQ(image.at(x, 20 + y), fromRgb565(rampTexel(wrapped, j))) << "wrap, column " << k;
    EXPECT_EQ(image.at(x, 170 + y), fromRgb565(rampTexel(clamped, j))) << "clamp, column " << k;
    EXPECT_EQ(image.at(x, 320 + y), fromRgb565(rampTexel(mirrored, j))) << "mirror, column " << k;
  }
}

// Between the texel centers the colors are interpolated, across the edge of
// the texture with the texel on the other side.
TEST_F(GlideCsim, FiltersTexturesBilinearly)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupTexturedTriangles();
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  const std::vector<FxU16> texture = rampTexture();

  const CsimStats before = getStats();
  grBufferClear(0xFF000000, 0, 0xFFFF);
  loadTexture(GR_LOD_LOG2_8, GR_LOD_LOG2_8, GR_TEXFMT_RGB_565, texture.data());
  drawTexturedRect(100.0f, 100.0f, 228.0f, 228.0f, 0.0f, 0.0f, 256.0f, 256.0f);
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  // Pixel x is at texel (x - 100 + 0.5) / 16, the centers at .5.
  auto redAt = [&](double texel) {
    const double u = texel - 0.5;
    const unsigned int i = static_cast<unsigned int>(u + 8) % 8;
    const double f = u - std::floor(u);
    return (1 - f) * red(fromRgb565(rampTexel(i, 2))) + f * red(fromRgb565(rampTexel((i + 1) % 8, 2)));
  };
  const unsigned int y = 100 + 16 * 2 + 8;
  for (unsigned int x : {100u, 140u, 147u, 160u, 227u}) {
    EXPECT_NEAR(red(image.at(x, y)), redAt((x - 100 + 0.5) / 16), 9.0) << "x " << x;
  }
  EXPECT_NE(image.at(147, y), image.at(140, y)) << "the same texel, not filtered";
}

// 1/w is 4 times larger at the bottom, so the middle of the texture is 1/5
// down the rectangle instead of halfway.
TEST_F(GlideCsim, MapsTexturesPerspectiveCorrect)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupTexturedTriangles();
  std::vector<FxU16> texture(8 * 8, 0xF800);
  std::fill(texture.begin() + 4 * 8, texture.end(), 0x001F);

  const CsimStats before = getStats();
  grBufferClear(0xFF000000, 0, 0xFFFF);
  loadTexture(GR_LOD_LOG2_8, GR_LOD_LOG2_8, GR_TEXFMT_RGB_565, texture.data());
  drawTexturedRect(100.0f, 100.0f, 300.0f, 300.0f, 0.0f, 0.0f, 256.0f, 256.0f, 0.25f, 1.0f);
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);

  // t = 256 f / (0.25 + 0.75 f) at f of the height is 128 at f = 0.2.
  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  EXPECT_EQ(image.at(200, 101), 0xFFFF0000u);
  EXPECT_EQ(image.at(200, 136), 0xFFFF0000u);
  EXPECT_EQ(image.at(200, 144), 0xFF0000FFu);
  EXPECT_EQ(image.at(200, 190), 0xFF0000FFu) << "interpolated without perspective";
  EXPECT_EQ(image.at(200, 299), 0xFF0000FFu);
}

// A 64 x 64 texture with a color per mipmap level, drawn at 48, 24 and 12
// pixels: 1.33, 2.67 and 5.33 texels per pixel select the 64, 32 and 16
// texel levels.
TEST_F(GlideCsim, SelectsMipmapLevels)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupTexturedTriangles();
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  const FxU16 levelColors[3] = {0xF800, 0x07E0, 0x001F};
  std::vector<FxU16> texture;
  for (unsigned int size = 64, level = 0; size >= 1; size /= 2, level++) {
    texture.insert(texture.end(), size * size, (level < 3) ? levelColors[level] : 0xFFFF);
  }

  const CsimStats before = getStats();
  grBufferClear(0xFF000000, 0, 0xFFFF);
  loadTexture(GR_LOD_LOG2_64, GR_LOD_LOG2_1, GR_TEXFMT_RGB_565, texture.data());
  drawTexturedRect(100.0f, 100.0f, 148.0f, 148.0f, 0.0f, 0.0f, 256.0f, 256.0f);
  drawTexturedRect(200.0f, 100.0f, 224.0f, 124.0f, 0.0f, 0.0f, 256.0f, 256.0f);
  drawTexturedRect(300.0f, 100.0f, 312.0f, 112.0f, 0.0f, 0.0f, 256.0f, 256.0f);
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  EXPECT_EQ(image.at(124, 124), 0xFFFF0000u) << "64 x 64";
  EXPECT_EQ(image.at(212, 112), 0xFF00FF00u) << "32 x 32";
  EXPECT_EQ(image.at(306, 106), 0xFF0000FFu) << "16 x 16";
}

// Every uncompressed format, with a different texel in each quadrant of the
// texture. Its color and its alpha are drawn side by side.
TEST_F(GlideCsim, DecodesTextureFormats)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  setupTexturedTriangles();

  GuTexPalette palette = {};
  palette.data[0] = 0xFF0000;
  palette.data[1] = 0x00FF00;
  palette.data[2] = 0x0000FF;
  palette.data[255] = 0x123456;
  GuTexPalette palette6666 = {};
  palette6666.data[0] = 0x80FF0000;
  palette6666.data[1] = 0xFF00FF00;
  palette6666.data[2] = 0x400000FF;
  palette6666.data[255] = 0xFF123456;
  // Y in steps of 17, I1 = (40, 0, -40) and Q2 = (0, 30, 0), the other I and
  // Q are 0.
  GuNccTable ncc = {};
  for (unsigned int k = 0; k < 16; k++) ncc.packed_data[k / 4] |= (17u * k) << (8 * (k % 4));
  auto iq = [](int r, int g, int b) {
    return (static_cast<FxU32>(r & 0x1FF) << 18) | (static_cast<FxU32>(g & 0x1FF) << 9) |
           static_cast<FxU32>(b & 0x1FF);
  };
  ncc.packed_data[4 + 1] = iq(40, 0, -40);
  ncc.packed_data[8 + 2] = iq(0, 30, 0);

  struct FormatCase {
    const char *name;
    GrTextureFormat_t format;
    unsigned int bytesPerTexel;
    unsigned int texels[4];    // top left, top right, bottom left, bottom right
    unsigned int expected[4];  // 0xAARRGGBB
    GrTexTable_t tableType;
    void *table;
  };
  const FormatCase cases[] = {
      {"RGB_332", GR_TEXFMT_RGB_332, 1, {0xE0, 0x1C, 0x03, 0x6D},
       {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFF6D6D55}, 0, nullptr},
      {"YIQ_422", GR_TEXFMT_YIQ_422, 1, {0xF4, 0xF0, 0x84, 0x52},
       {0xFFFFFFD7, 0xFFFFFFFF, 0xFFB08860, 0xFF557355}, GR_TEXTABLE_NCC0, &ncc},
      {"ALPHA_8", GR_TEXFMT_ALPHA_8, 1, {0x00, 0x40, 0x80, 0xFF},
       {0x00000000, 0x40404040, 0x80808080, 0xFFFFFFFF}, 0, nullptr},
      {"INTENSITY_8", GR_TEXFMT_INTENSITY_8, 1, {0x00, 0x40, 0x80, 0xFF},
       {0xFF000000, 0xFF404040, 0xFF808080, 0xFFFFFFFF}, 0, nullptr},
      {"ALPHA_INTENSITY_44", GR_TEXFMT_ALPHA_INTENSITY_44, 1, {0xF0, 0x0F, 0x5A, 0xA5},
       {0xFF000000, 0x00FFFFFF, 0x55AAAAAA, 0xAA555555}, 0, nullptr},
      {"P_8", GR_TEXFMT_P_8, 1, {0x00, 0x01, 0x02, 0xFF},
       {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFF123456}, GR_TEXTABLE_PALETTE, &palette},
      {"P_8_6666", GR_TEXFMT_P_8_6666, 1, {0x00, 0x01, 0x02, 0xFF},
       {0x82FF0000, 0xFF00FF00, 0x410000FF, 0xFF103455}, GR_TEXTABLE_PALETTE_6666_EXT,
       &palette6666},
      {"ARGB_8332", GR_TEXFMT_ARGB_8332, 2, {0x80E0, 0x401C, 0xFF03, 0x006D},
       {0x80FF0000, 0x4000FF00, 0xFF0000FF, 0x006D6D55}, 0, nullptr},
      {"AYIQ_8422", GR_TEXFMT_AYIQ_8422, 2, {0x80F4, 0x40F0, 0xFF84, 0x0052},
       {0x80FFFFD7, 0x40FFFFFF, 0xFFB08860, 0x00557355}, GR_TEXTABLE_NCC0, &ncc},
      {"RGB_565", GR_TEXFMT_RGB_565, 2, {0xF800, 0x07E0, 0x001F, 0x8410},
       {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFF848284}, 0, nullptr},
      {"ARGB_1555", GR_TEXFMT_ARGB_1555, 2, {0xFC00, 0x03E0, 0x801F, 0x4210},
       {0xFFFF0000, 0x0000FF00, 0xFF0000FF, 0x00848484}, 0, nullptr},
      {"ARGB_4444", GR_TEXFMT_ARGB_4444, 2, {0xFF00, 0x80F0, 0x400F, 0x0123},
       {0xFFFF0000, 0x8800FF00, 0x440000FF, 0x00112233}, 0, nullptr},
      {"ALPHA_INTENSITY_88", GR_TEXFMT_ALPHA_INTENSITY_88, 2, {0xFF00, 0x00FF, 0x8040, 0x4080},
       {0xFF000000, 0x00FFFFFF, 0x80404040, 0x40808080}, 0, nullptr},
      {"AP_88", GR_TEXFMT_AP_88, 2, {0x8000, 0x4001, 0xFF02, 0x00FF},
       {0x80FF0000, 0x4000FF00, 0xFF0000FF, 0x00123456}, GR_TEXTABLE_PALETTE, &palette},
      {"ARGB_8888", kTexFmtArgb8888, 4, {0x80FF0000, 0x4000FF00, 0xFF0000FF, 0x00123456},
       {0x80FF0000, 0x4000FF00, 0xFF0000FF, 0x00123456}, 0, nullptr},
  };

  const CsimStats before = getStats();
  for (const FormatCase &c : cases) {
    SCOPED_TRACE(c.name);
    std::vector<FxU8> texture(8 * 8 * c.bytesPerTexel);
    for (unsigned int j = 0; j < 8; j++) {
      for (unsigned int i = 0; i < 8; i++) {
        const unsigned int texel = c.texels[(j / 4) * 2 + i / 4];
        for (unsigned int b = 0; b < c.bytesPerTexel; b++) {
          texture[(j * 8 + i) * c.bytesPerTexel + b] = static_cast<FxU8>(texel >> (8 * b));
        }
      }
    }
    if (c.table != nullptr) grTexDownloadTable(c.tableType, c.table);

    grBufferClear(0xFF000000, 0, 0xFFFF);
    loadTexture(GR_LOD_LOG2_8, GR_LOD_LOG2_8, c.format, texture.data());
    showTextureColor();
    drawTexturedRect(100.0f, 100.0f, 164.0f, 164.0f, 0.0f, 0.0f, 256.0f, 256.0f);
    showTextureAlpha();
    drawTexturedRect(300.0f, 100.0f, 364.0f, 164.0f, 0.0f, 0.0f, 256.0f, 256.0f);
    grBufferSwap(1);
    grFinish();

    const SwappedImage image(0);
    ASSERT_TRUE(image.valid());
    for (unsigned int q = 0; q < 4; q++) {
      const unsigned int x = 16 + 32 * (q % 2), y = 116 + 32 * (q / 2);
      const unsigned int alpha = c.expected[q] >> 24;
      EXPECT_EQ(image.at(100 + x, y), as565(c.expected[q])) << "color of texel " << q;
      EXPECT_EQ(image.at(300 + x, y), as565(alpha * 0x010101)) << "alpha of texel " << q;
    }
  }
  const CsimStats after = getStats();

  EXPECT_EQ(after.fifoErrors, before.fifoErrors);
  EXPECT_EQ(after.unsimulated, before.unsimulated);
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

// The stride of the lock is the one of the color buffer, so the pixel of
// row y appears in row y of the swapped image.
TEST_F(GlideCsim, WritesPixelsThroughTheLockedFrameBuffer)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  grBufferClear(0xFF000000, 0, 0xFFFF);

  GrLfbInfo_t write;
  write.size = sizeof(write);
  ASSERT_TRUE(grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_565,
                        GR_ORIGIN_UPPER_LEFT, FXFALSE, &write));
  ASSERT_EQ(write.writeMode, GR_LFBWRITEMODE_565);
  for (FxU32 y = 100; y < 110; y++) {
    auto *row = reinterpret_cast<FxU16 *>(static_cast<FxU8 *>(write.lfbPtr) + y * write.strideInBytes);
    for (FxU32 x = 200; x < 220; x++) row[x] = 0xF800;
  }
  EXPECT_TRUE(grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER));
  grBufferSwap(1);
  grFinish();

  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  EXPECT_EQ(image.count(0xFFFF0000), 10u * 20u);
  EXPECT_EQ(image.at(200, 100), 0xFFFF0000u);
  EXPECT_EQ(image.at(219, 109), 0xFFFF0000u);
}

// grLfbWriteRegion writes through the 3D LFB, which converts the source
// format into the one of the color buffer; grLfbReadRegion reads it back.
TEST_F(GlideCsim, WritesRegionsThroughThe3dLfb)
{
  ASSERT_NO_FATAL_FAILURE(openWindow());
  grDitherMode(GR_DITHER_DISABLE);
  grBufferClear(0xFF000000, 0, 0xFFFF);

  const CsimStats before = getStats();
  std::vector<FxU16> green(16 * 8, 0x07E0);
  EXPECT_TRUE(grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 100, 50, GR_LFB_SRC_FMT_565, 16, 8, FXFALSE,
                               16 * sizeof(FxU16), green.data()));
  std::vector<FxU32> blue(8 * 4, 0x000000FF);
  EXPECT_TRUE(grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 300, 200, GR_LFB_SRC_FMT_888, 8, 4, FXFALSE,
                               8 * sizeof(FxU32), blue.data()));
  std::vector<FxU16> readBack(16 * 8, 0);
  EXPECT_TRUE(grLfbReadRegion(GR_BUFFER_BACKBUFFER, 100, 50, 16, 8, 16 * sizeof(FxU16),
                              readBack.data()));
  grBufferSwap(1);
  grFinish();
  const CsimStats after = getStats();

  EXPECT_EQ(after.unsimulated, before.unsimulated);
  EXPECT_EQ(readBack, green);
  const SwappedImage image(0);
  ASSERT_TRUE(image.valid());
  EXPECT_EQ(image.count(0xFF00FF00), 16u * 8u);
  EXPECT_EQ(image.at(100, 50), 0xFF00FF00u);
  EXPECT_EQ(image.at(115, 57), 0xFF00FF00u);
  EXPECT_EQ(image.count(0xFF0000FF), 8u * 4u);
  EXPECT_EQ(image.at(307, 203), 0xFF0000FFu);
}

}  // namespace
