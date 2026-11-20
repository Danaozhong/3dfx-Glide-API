#include <algorithm>

#include "fixed_point.hpp"
#include "rasterization.hpp"

namespace csim {

namespace {

/// @brief Largest pixel coordinate of the rasterizer, SST_XY_INTBITS signed.
constexpr int kMaxCoordinate = 2047;

/// @brief Z bits of the 16bpp iterator.
constexpr FxI64 kZ64Mask16Bpp = 0x0000FFFFFFFF0000;

/**
 * @brief The first scanline whose center is at or above @p y.
 * @param[in] y A y coordinate, 12.4.
 */
int scanlineAtOrAbove(FxI32 y)
{
  return (y + (1 << (SST_XY_FRACBITS - 1)) - 1) >> SST_XY_FRACBITS;
}

/**
 * @brief A vertex coordinate register, 12.4.
 * @param[in] reg The value of the register.
 */
FxI32 coordinate(FxU32 reg) { return signExtend(reg, SST_XY_SIZE); }

/**
 * @brief An edge, with the error term at the center of the pixel of the
 * start (_setupEdge).
 * @param[in] x0 X of the start, 12.4.
 * @param[in] y0 Y of the start, 12.4.
 * @param[in] x1 X of the end, 12.4.
 * @param[in] y1 Y of the end, 12.4.
 */
Edge setupEdge(FxI32 x0, FxI32 y0, FxI32 x1, FxI32 y1)
{
  Edge edge;
  edge.dedy = x0 - x1;
  edge.dedx = y1 - y0;
  edge.e = (edge.dedx * (0x8 - (x0 & 0xF)) + edge.dedy * (0x8 - (y0 & 0xF))) >> SST_XY_FRACBITS;
  edge.x0 = x0 >> SST_XY_FRACBITS;
  edge.y0 = y0 >> SST_XY_FRACBITS;
  return edge;
}

/**
 * @brief Sign bit of the 28.4 edge error term at the center of a pixel.
 * @param[in] edge The edge.
 * @param[in] x Pixel column.
 * @param[in] y Pixel row.
 */
FxU32 edgeSign(const Edge &edge, int x, int y)
{
  const FxI64 e = FxI64{edge.e} + FxI64{x - edge.x0} * edge.dedx + FxI64{y - edge.y0} * edge.dedy;
  return static_cast<FxU32>(e) & 0x80000000u;
}

/**
 * @brief Sub-pixel correction of a 12.12 parameter.
 * @param[in] dpdx Gradient of the parameter in x.
 * @param[in] dpdy Gradient of the parameter in y.
 * @param[in] dx Distance in x, .4 format.
 * @param[in] dy Distance in y, .4 format.
 */
FxI32 adjust(FxI32 dpdx, FxI32 dpdy, int dx, int dy)
{
  return ((dx * signExtend(dpdx, SST_RGBA_SIZE)) >> SST_XY_FRACBITS) +
         ((dy * signExtend(dpdy, SST_RGBA_SIZE)) >> SST_XY_FRACBITS);
}

/**
 * @brief Sub-pixel correction of Z along one axis.
 * @param[in] dz Gradient of Z along the axis.
 * @param[in] d Distance along the axis, .4 format.
 */
FxI64 adjustZ(FxI64 dz, int d)
{
  return (d * signExtend64(dz & mask64(kZIteratorBits), kZIteratorBits)) >> SST_XY_FRACBITS;
}

/**
 * @brief A 12.12 parameter, @p dx and @p dy pixels from @p start.
 * @param[in] start The parameter at the start pixel.
 * @param[in] dpdx Gradient in x.
 * @param[in] dpdy Gradient in y.
 * @param[in] dx Pixels in x from the start pixel.
 * @param[in] dy Pixels in y from the start pixel.
 */
FxI32 iterate(FxI32 start, FxI32 dpdx, FxI32 dpdy, int dx, int dy)
{
  const FxI64 p = FxI64{start} + FxI64{dx} * signExtend(dpdx, SST_RGBA_SIZE) +
                  FxI64{dy} * signExtend(dpdy, SST_RGBA_SIZE);
  return static_cast<FxI32>(static_cast<FxU32>(p) & mask(SST_RGBA_SIZE));
}

/**
 * @brief Z, @p dx and @p dy pixels from @p start, wrapping around at 48 bits
 * like the iterator.
 * @param[in] start Z at the start pixel.
 * @param[in] dzdx Gradient in x.
 * @param[in] dzdy Gradient in y.
 * @param[in] dx Pixels in x from the start pixel.
 * @param[in] dy Pixels in y from the start pixel.
 */
FxI64 iterateZ(FxI64 start, FxI64 dzdx, FxI64 dzdy, int dx, int dy)
{
  const FxU64 z = static_cast<FxU64>(start) + static_cast<FxU64>(FxI64{dx}) * static_cast<FxU64>(dzdx) +
                  static_cast<FxU64>(FxI64{dy}) * static_cast<FxU64>(dzdy);
  return static_cast<FxI64>(z) & mask64(kZIteratorBits);
}

}  // namespace

void Rasterizer::setupTriangle(const RenderState &state, ZIterator &z)
{
  SstRegs &s = context_.sst();
  const bool is32Bpp = state.format == PixelFormat::Argb8888;

  const FxI32 xA = coordinate(s.vA.x);
  const FxI32 yA = coordinate(s.vA.y);
  start_.x = xA >> SST_XY_FRACBITS;
  start_.y = yA >> SST_XY_FRACBITS;
  const int dx = 0x8 - (xA & 0xF);  // .4 distance to the pixel center
  const int dy = 0x8 - (yA & 0xF);

  Fragment &p = start_;
  p.r = s.r;
  p.g = s.g;
  p.b = s.b;
  p.a = s.a;
  p.z64 = z.start;
  if (s.fbzColorPath & SST_PARMADJUST) {
    p.r += adjust(s.drdx, s.drdy, dx, dy);
    p.g += adjust(s.dgdx, s.dgdy, dx, dy);
    p.b += adjust(s.dbdx, s.dbdy, dx, dy);
    p.a += adjust(s.dadx, s.dady, dx, dy);
    if (!is32Bpp) {
      p.z64 &= kZ64Mask16Bpp;
      z.dzdx &= kZ64Mask16Bpp;
      z.dzdy &= kZ64Mask16Bpp;
    }
    p.z64 += adjustZ(z.dzdx, dx);
    if (!is32Bpp) p.z64 &= kZ64Mask16Bpp;
    p.z64 += adjustZ(z.dzdy, dy);

    s.r = static_cast<FxI32>(p.r & mask(SST_RGBA_SIZE));
    s.g = static_cast<FxI32>(p.g & mask(SST_RGBA_SIZE));
    s.b = static_cast<FxI32>(p.b & mask(SST_RGBA_SIZE));
    s.a = static_cast<FxI32>(p.a & mask(SST_RGBA_SIZE));
    z.start = p.z64;
  }
  p.r &= mask(SST_RGBA_SIZE);
  p.g &= mask(SST_RGBA_SIZE);
  p.b &= mask(SST_RGBA_SIZE);
  p.a &= mask(SST_RGBA_SIZE);
  p.z64 &= mask64(kZIteratorBits);
  if (!is32Bpp) p.z64 &= kZ64Mask16Bpp;
  dzdx_ = z.dzdx;
  dzdy_ = z.dzdy;

  edge0_ = setupEdge(xA, yA, coordinate(s.vC.x), coordinate(s.vC.y));
  edge1_ = setupEdge(xA, yA, coordinate(s.vB.x), coordinate(s.vB.y));
}

void Rasterizer::triangle(const RenderState &state, ZIterator &z)
{
  setupTriangle(state, z);

  SstRegs &s = context_.sst();
  s.fbiTrianglesOut = (s.fbiTrianglesOut + 1) & 0xFFFFFF;

  const FxI32 yA = coordinate(s.vA.y);
  const FxI32 yB = coordinate(s.vB.y);
  const FxI32 yC = coordinate(s.vC.y);
  const Edge edgeBC = setupEdge(coordinate(s.vB.x), yB, coordinate(s.vC.x), yC);

  int yStart = std::max(scanlineAtOrAbove(yA), 0);
  int yEnd = std::min(scanlineAtOrAbove(yC), kMaxCoordinate + 1);
  const int yMid = scanlineAtOrAbove(yB);
  if (state.clip) {
    yStart = std::max(yStart, state.clipRect.yMin);
    yEnd = std::min(yEnd, state.clipRect.yMax);
  }

  for (int y = yStart; y < yEnd; y++) {
    span(y, (y < yMid) ? edge1_ : edgeBC, state);
  }
}

void Rasterizer::span(int y, const Edge &edge1, const RenderState &state)
{
  const SstRegs &s = context_.sst();
  const FxU32 minor = s.triangleCMD & 0x80000000u;
  const FxU32 major = minor ^ 0x80000000u;

  const int x0 = coordinate(s.vA.x) >> SST_XY_FRACBITS;
  const int x1 = coordinate(s.vB.x) >> SST_XY_FRACBITS;
  const int x2 = coordinate(s.vC.x) >> SST_XY_FRACBITS;
  int xStart = std::max(std::min({x0, x1, x2}) - 1, 0);
  int xEnd = std::min(std::max({x0, x1, x2}) + 2, kMaxCoordinate + 1);
  if (state.clip) {
    xStart = std::max(xStart, state.clipRect.xMin);
    xEnd = std::min(xEnd, state.clipRect.xMax);
  }

  const FxI32 drdx = s.drdx, dgdx = s.dgdx, dbdx = s.dbdx, dadx = s.dadx;
  const FxI32 drdy = s.drdy, dgdy = s.dgdy, dbdy = s.dbdy, dady = s.dady;
  const int dy = y - start_.y;

  for (int x = xStart; x < xEnd; x++) {
    const FxU32 in0 = major ^ edgeSign(edge0_, x, y);
    const FxU32 in1 = minor ^ edgeSign(edge1, x, y);
    if (!(in0 & in1)) continue;

    // sstPixel
    if (state.needsUnsimulated) context_.stats->unsimulated++;
    if (!rectClip(x, y, state)) continue;

    const int dx = x - start_.x;
    Fragment f;
    f.x = x;
    f.y = y;
    f.r = iterate(start_.r, drdx, drdy, dx, dy);
    f.g = iterate(start_.g, dgdx, dgdy, dx, dy);
    f.b = iterate(start_.b, dbdx, dbdy, dx, dy);
    f.a = iterate(start_.a, dadx, dady, dx, dy);
    f.z64 = iterateZ(start_.z64, dzdx_, dzdy_, dx, dy);
    fbiPixelPipeline_.shade(f, state);
  }
}

bool Rasterizer::rectClip(int x, int y, const RenderState &state) const
{
  SstRegs &s = context_.sst();
  s.stats.fbiPixelsIn = (s.stats.fbiPixelsIn + 1) & 0xFFFFFF;

  if (!state.clip) return true;
  if (!state.clipRect.contains(x, y)) return false;
  if (state.clip1 && (state.clip1Rect.contains(x, y) == state.clip1Exclusive)) return false;
  return true;
}

void Rasterizer::fastFill(const RenderState &state)
{
  const SstRegs &s = context_.sst();
  constexpr FxU32 kMask = 0x7FFF;
  const int xStart = static_cast<int>((s.clipLeftRight >> 16) & kMask);
  const int xStop = static_cast<int>(s.clipLeftRight & kMask);
  const int yStart = static_cast<int>((s.clipBottomTop >> 16) & kMask);
  const int yStop = static_cast<int>(s.clipBottomTop & kMask);

  for (int y = yStart; y < yStop; y++) {
    for (int x = xStart; x < xStop; x++) fbiPixelPipeline_.fill(x, y, state);
  }
  context_.stats->fastFills++;
}

}  // namespace csim
