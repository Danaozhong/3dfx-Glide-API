#include "teapot.hpp"

#include <cmath>

#include "teapot_data.h"

namespace teapot {

namespace {

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator*(float s, Vec3 v) { return {s * v.x, s * v.y, s * v.z}; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float length(Vec3 v) { return std::sqrt(dot(v, v)); }

// Cubic Bernstein polynomials and their derivatives at t.
void bernstein(float t, float b[4], float db[4])
{
  const float s = 1.0f - t;
  b[0] = s * s * s;
  b[1] = 3.0f * t * s * s;
  b[2] = 3.0f * t * t * s;
  b[3] = t * t * t;
  db[0] = -3.0f * s * s;
  db[1] = 3.0f * s * s - 6.0f * t * s;
  db[2] = 6.0f * t * s - 3.0f * t * t;
  db[3] = 3.0f * t * t;
}

struct Patch {
  Vec3 p[4][4];

  void evaluate(float u, float v, Vec3 &position, Vec3 &dpdu, Vec3 &dpdv) const
  {
    float bu[4], dbu[4], bv[4], dbv[4];
    bernstein(u, bu, dbu);
    bernstein(v, bv, dbv);
    position = dpdu = dpdv = Vec3{0, 0, 0};
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        position = position + (bu[i] * bv[j]) * p[i][j];
        dpdu = dpdu + (dbu[i] * bv[j]) * p[i][j];
        dpdv = dpdv + (bu[i] * dbv[j]) * p[i][j];
      }
    }
  }

  // The rows of the lid top and the bottom collapse into a point, where the
  // normal is taken from slightly inside of the patch.
  Vec3 normal(float u, float v) const
  {
    Vec3 position, dpdu, dpdv;
    for (float nudge : {0.0f, 1e-3f, 1e-2f}) {
      evaluate((u < 0.5f) ? u + nudge : u - nudge, (v < 0.5f) ? v + nudge : v - nudge, position,
               dpdu, dpdv);
      const Vec3 n = cross(dpdu, dpdv);
      const float len = length(n);
      if (len > 1e-6f) return (1.0f / len) * n;
    }
    return Vec3{0, 1, 0};
  }
};

// Data patch 'index' mirrored by sx, sy, in the y up coordinates of Mesh.
// Mirroring once reverses the orientation, so the column order is reversed
// too, which keeps the normals pointing outward.
Patch makePatch(int index, float sx, float sy)
{
  const bool reverse = (sx * sy) < 0;
  Patch patch;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      const float *c = kControlPoints[kPatches[index][i * 4 + (reverse ? 3 - j : j)]];
      patch.p[i][j] = Vec3{sx * c[0], c[2], -sy * c[1]};
    }
  }
  return patch;
}

}  // namespace

Mesh tessellate(int subdivisions)
{
  struct Copy {
    int index;
    float sx, sy;
  };
  std::vector<Copy> copies;
  for (int i = 0; i < kPatchCount; i++) {
    copies.push_back({i, 1, 1});
    copies.push_back({i, 1, -1});
    if (i < kMirroredPatches) {
      copies.push_back({i, -1, 1});
      copies.push_back({i, -1, -1});
    }
  }

  Mesh mesh;
  mesh.subdivisions = subdivisions;
  mesh.patches = static_cast<int>(copies.size());
  for (const Copy &copy : copies) {
    const Patch patch = makePatch(copy.index, copy.sx, copy.sy);
    for (int row = 0; row <= subdivisions; row++) {
      for (int column = 0; column <= subdivisions; column++) {
        const float u = static_cast<float>(row) / static_cast<float>(subdivisions);
        const float v = static_cast<float>(column) / static_cast<float>(subdivisions);
        Vec3 position, dpdu, dpdv;
        patch.evaluate(u, v, position, dpdu, dpdv);
        mesh.positions.push_back(position);
        mesh.normals.push_back(patch.normal(u, v));
      }
    }
  }

  // The patches of the data are oriented the same way, inward or outward.
  // A point on the side of the body tells which.
  const int probe = mesh.index(4, subdivisions / 2, 0);  // body, after the 4 copies of the rim
  const Vec3 side{mesh.positions[probe].x, 0, mesh.positions[probe].z};
  if (dot(mesh.normals[probe], side) < 0) {
    for (Vec3 &n : mesh.normals) n = -1.0f * n;
  }
  return mesh;
}

}  // namespace teapot
