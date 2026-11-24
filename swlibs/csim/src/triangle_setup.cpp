#include <cmath>
#include <cstring>
#include <utility>

#include "fixed_point.hpp"
#include "triangle_setup.hpp"

namespace csim {

namespace {

/**
 * @brief The float with the given bits.
 * @param[in] bits The bits of the float.
 */
float floatOf(FxU32 bits)
{
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

/**
 * @brief The bits of a float.
 * @param[in] value The float.
 */
FxU32 bitsOf(float value)
{
  FxU32 bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

/**
 * @brief An x or y register, snapped to .4 precision by truncation like the
 * mantissa truncation of the hardware.
 * @param[in] bits The bits of the float register.
 */
float snapXY(FxU32 bits)
{
  const float value = floatOf(bits);
  if (std::fabs(value) < 1.0f / 16) return 0.0f;
  return std::trunc(value * 16) / 16;
}

/**
 * @brief Denormals as 0, like the float units.
 * @param[in] value The float.
 */
float flushDenormal(float value)
{
  return (std::fpclassify(value) == FP_SUBNORMAL) ? 0.0f : value;
}

/**
 * @brief Float to fixed point, truncated toward 0 and saturated at 2^62.
 * @param[in] value The float.
 * @param[in] fracBits Fraction bits of the fixed point value.
 */
FxI64 toFixed64(float value, int fracBits)
{
  constexpr double kLimit = 4611686018427387904.0;  // 2^62
  const double fixed = std::ldexp(static_cast<double>(value), fracBits);
  if (std::isnan(fixed)) return 0;
  if (fixed >= kLimit) return static_cast<FxI64>(kLimit);
  if (fixed <= -kLimit) return -static_cast<FxI64>(kLimit);
  return static_cast<FxI64>(fixed);
}

/**
 * @brief Float to a fixed point register.
 * @param[in] value The float.
 * @param[in] fracBits Fraction bits of the register.
 * @param[in] size Width of the register in bits.
 */
FxU32 toFixed(float value, int fracBits, int size)
{
  return static_cast<FxU32>(toFixed64(value, fracBits)) & mask(size);
}

}  // namespace

void SetupUnit::beginTriangle()
{
  vertexCount_ = 0;
  pingpong_ = false;
  copyVertex();
}

bool SetupUnit::drawTriangle(ZIterator &z)
{
  vertexCount_++;
  if (vertexCount_ > 2) {
    if (!(context_.sst().sSetupMode & SST_SETUP_FAN)) {
      vArray_[0] = vArray_[1];
      pingpong_ = !pingpong_;
    }
    vArray_[1] = vArray_[2];
    vertexCount_ = 2;
  }
  copyVertex();
  return (vertexCount_ == 2) && setup(z);
}

void SetupUnit::copyVertex()
{
  const SstRegs &s = context_.sst();
  SetupVertex &p = vArray_[vertexCount_];
  p.x = snapXY(s.sVx);
  p.y = snapXY(s.sVy);
  p.z = floatOf(s.sVz);
  p.w = floatOf(s.sOowfbi);
  p.r = floatOf(s.sRed);
  p.g = floatOf(s.sGreen);
  p.b = floatOf(s.sBlue);
  p.a = floatOf(s.sAlpha);
  p.w0 = floatOf(s.sOow0);
  p.s0 = floatOf(s.sSow0);
  p.t0 = floatOf(s.sTow0);
  p.w1 = floatOf(s.sOow1);
  p.s1 = floatOf(s.sSow1);
  p.t1 = floatOf(s.sTow1);
}

bool SetupUnit::setup(ZIterator &z)
{
  SstRegs &s = context_.sst();
  const FxU32 smode = s.sSetupMode;
  const SetupVertex *const v = vArray_;

  // Area first, like the hardware, and cull.
  const float dxAB = v[0].x - v[1].x;
  const float dyAB = v[0].y - v[1].y;
  const float dxBC = v[1].x - v[2].x;
  const float dyBC = v[1].y - v[2].y;
  const float area = dxAB * dyBC - dxBC * dyAB;
  if (area == 0.0f) return false;

  if (smode & SST_SETUP_EN_CULLING) {
    // Every other triangle of a strip has the opposite winding.
    const bool cullNegative = (smode & SST_SETUP_CULL_NEGATIVE) != 0;
    const bool flipped = pingpong_ && !(smode & SST_SETUP_DIS_PINGPONG);
    if (std::signbit(area) == (cullNegative != flipped)) return false;
  }
  const float ooarea = 1.0f / area;

  // The rasterizer walks the vertices from low to high y, on equal y the
  // later vertex first. Swapping two vertices reverses the winding, so every
  // swap negates the area.
  int order[3] = {0, 1, 2};
  float sortedArea = area;
  auto sortPair = [&](int i, int j) {
    const int p = order[i], q = order[j];
    if (v[q].y < v[p].y || (v[q].y == v[p].y && q > p)) {
      std::swap(order[i], order[j]);
      sortedArea = -sortedArea;
    }
  };
  sortPair(0, 1);
  sortPair(1, 2);
  sortPair(0, 1);
  const int a = order[0], b = order[1], c = order[2];
  s.triangleCMD = bitsOf(sortedArea);  // the rasterizer needs only the sign

  // Gradients from the unsorted vertices.
  auto gradient = [&](float SetupVertex::*p, float &dpdx, float &dpdy) {
    const float p0 = flushDenormal(v[0].*p);
    const float p1 = flushDenormal(v[1].*p);
    const float p2 = flushDenormal(v[2].*p);
    const float dpAB = p0 - p1;
    const float dpBC = p1 - p2;
    dpdx = (dpAB * dyBC - dpBC * dyAB) * ooarea;
    dpdy = (dpBC * dxAB - dpAB * dxBC) * ooarea;
  };
  auto setupParameter = [&](float SetupVertex::*p, int fracBits, int size, volatile FxI32 &start,
                            volatile FxI32 &dpdxReg, volatile FxI32 &dpdyReg) {
    float dpdx, dpdy;
    gradient(p, dpdx, dpdy);
    start = static_cast<FxI32>(toFixed(v[a].*p, fracBits, size));
    dpdxReg = static_cast<FxI32>(toFixed(dpdx, fracBits, size));
    dpdyReg = static_cast<FxI32>(toFixed(dpdy, fracBits, size));
  };
  auto setupRGBA = [&](float SetupVertex::*p, volatile FxI32 &start, volatile FxI32 &dpdxReg,
                       volatile FxI32 &dpdyReg) {
    setupParameter(p, SST_RGBA_FRACBITS, SST_RGBA_SIZE, start, dpdxReg, dpdyReg);
  };

  s.vA.x = toFixed(v[a].x, SST_XY_FRACBITS, SST_XY_SIZE);
  s.vA.y = toFixed(v[a].y, SST_XY_FRACBITS, SST_XY_SIZE);
  s.vB.x = toFixed(v[b].x, SST_XY_FRACBITS, SST_XY_SIZE);
  s.vB.y = toFixed(v[b].y, SST_XY_FRACBITS, SST_XY_SIZE);
  s.vC.x = toFixed(v[c].x, SST_XY_FRACBITS, SST_XY_SIZE);
  s.vC.y = toFixed(v[c].y, SST_XY_FRACBITS, SST_XY_SIZE);

  if (smode & SST_SETUP_RGB) {
    setupRGBA(&SetupVertex::r, s.r, s.drdx, s.drdy);
    setupRGBA(&SetupVertex::g, s.g, s.dgdx, s.dgdy);
    setupRGBA(&SetupVertex::b, s.b, s.dbdx, s.dbdy);
  }
  if (smode & SST_SETUP_A) {
    setupRGBA(&SetupVertex::a, s.a, s.dadx, s.dady);
  }
  if (smode & SST_SETUP_Z) {
    // In 16bpp with 12 fraction bits, then aligned to the 28 of the iterator.
    const bool is32Bpp = (s.renderMode & SST_RM_3D_MODE) == SST_RM_32BPP;
    const int fracBits = is32Bpp ? kZIteratorFracBits32Bpp : SST_Z_16BPP_FRACBITS;
    const int shift = is32Bpp ? 0 : kZIteratorFracBits16Bpp - SST_Z_16BPP_FRACBITS;
    float dzdx, dzdy;
    gradient(&SetupVertex::z, dzdx, dzdy);
    auto toZ64 = [&](float value) {
      return static_cast<FxI64>(static_cast<FxU64>(toFixed64(value, fracBits) & mask64(kZIteratorBits))
                                << shift);
    };
    z.start = toZ64(v[a].z);
    z.dzdx = toZ64(dzdx);
    z.dzdy = toZ64(dzdy);
  }
  if (smode & SST_SETUP_Wfbi) {
    setupParameter(&SetupVertex::w, SST_W_FRACBITS, SST_W_SIZE, s.w, s.dwdx, s.dwdy);
  }

  // A TMU without its own W, or S and T, takes those of the unit before it.
  float SetupVertex::*w = (smode & SST_SETUP_W0) ? &SetupVertex::w0 : &SetupVertex::w;
  float SetupVertex::*sow = &SetupVertex::s0;
  float SetupVertex::*tow = &SetupVertex::t0;
  for (int t = 0; t < kNumTmus; t++) {
    if ((t == 1) && (smode & SST_SETUP_W1)) w = &SetupVertex::w1;
    if ((t == 1) && (smode & SST_SETUP_ST1)) {
      sow = &SetupVertex::s1;
      tow = &SetupVertex::t1;
    }
    SstRegs &tmu = context_.tmu(t);
    if (smode & (SST_SETUP_Wfbi | SST_SETUP_W0 | SST_SETUP_W1)) {
      setupParameter(w, SST_W_FRACBITS, SST_W_SIZE, tmu.w, tmu.dwdx, tmu.dwdy);
    }
    if (smode & (SST_SETUP_ST0 | SST_SETUP_ST1)) {
      setupParameter(sow, SST_ST_FRACBITS, SST_ST_SIZE, tmu.s, tmu.dsdx, tmu.dsdy);
      setupParameter(tow, SST_ST_FRACBITS, SST_ST_SIZE, tmu.t, tmu.dtdx, tmu.dtdy);
    }
  }
  return true;
}

}  // namespace csim
