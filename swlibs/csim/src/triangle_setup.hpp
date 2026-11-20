/**
 * @file
 * @brief Setup unit (TSU): turns the float vertices of the sV* registers
 * into the start values and gradients of a triangle.
 */
#ifndef CSIM_TRIANGLE_SETUP_HPP
#define CSIM_TRIANGLE_SETUP_HPP

#include "chip_context.hpp"
#include "rasterization.hpp"

namespace csim {

/// @brief A vertex, as written into the sV* registers.
struct SetupVertex {
  float x, y, z, w;
  float r, g, b, a;
};

/// @brief The setup unit.
class SetupUnit {
public:
  /// @param[in] context The registers of the chip.
  explicit SetupUnit(const ChipContext &context) : context_(context) {}

  /**
   * @brief sBeginTriCMD: starts a strip or fan with the vertex in the
   * registers.
   */
  void beginTriangle();

  /**
   * @brief sDrawTriCMD: adds the vertex in the registers.
   * @param[out] z The Z iterator of the triangle.
   * @return True if there is a triangle to draw, after its parameters are in
   * the vA, vB, vC, color and triangleCMD registers, and its Z in @p z.
   */
  bool drawTriangle(ZIterator &z);

private:
  /// @brief Copies the sV* registers into the next vertex.
  void copyVertex();
  /**
   * @brief Sets up the triangle of the 3 vertices.
   * @param[out] z The Z iterator of the triangle.
   * @return False if it is culled.
   */
  bool setup(ZIterator &z);

  const ChipContext &context_;
  SetupVertex vArray_[3] = {};
  int vertexCount_ = 0;
  bool pingpong_ = false;  ///< Flips for every strip triangle.
};

}  // namespace csim

#endif  // CSIM_TRIANGLE_SETUP_HPP
