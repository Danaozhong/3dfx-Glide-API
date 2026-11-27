// The Utah teapot as a triangle mesh, tessellated from its Bezier patches.
#ifndef CSIM_TEAPOT_TEAPOT_HPP
#define CSIM_TEAPOT_TEAPOT_HPP

#include <vector>

namespace teapot {

struct Vec3 {
  float x, y, z;
};

// 'patches' grids of (subdivisions + 1)^2 vertices each, row by row. The
// teapot stands on the plane y = 0, with y up, and is about 6.4 wide.
// Normals point outward.
struct Mesh {
  int subdivisions = 0;
  int patches = 0;
  std::vector<Vec3> positions;
  std::vector<Vec3> normals;

  int rowLength() const { return subdivisions + 1; }
  int index(int patch, int row, int column) const
  {
    return (patch * rowLength() + row) * rowLength() + column;
  }
};

Mesh tessellate(int subdivisions);

}  // namespace teapot

#endif  // CSIM_TEAPOT_TEAPOT_HPP
