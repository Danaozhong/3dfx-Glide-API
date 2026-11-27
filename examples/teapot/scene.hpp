// A frame of the teapot demo, drawn with Glide: a sky gradient, a checkered
// floor with the shadow of the teapot, and the teapot, lit per vertex on the
// CPU like every Glide application does. Everything is Gouraud shaded and
// depth buffered, without textures, fog or alpha blending.
#ifndef CSIM_TEAPOT_SCENE_HPP
#define CSIM_TEAPOT_SCENE_HPP

#include <vector>

#include "teapot.hpp"

namespace teapot {

class Scene {
public:
  explicit Scene(int subdivisions = 10);

  // Draws into the back buffer of the current Glide context, with the
  // teapot turned by 'angle' degrees around its vertical axis.
  void draw(float angle, float width, float height);

private:
  struct Vertex {
    float x, y, z;  // screen position, z in 16-bit depth buffer units
    float r, g, b;
  };

  void setupState();
  Vertex project(Vec3 p, Vec3 color) const;
  void drawBackground();
  void drawFloor();
  void drawMesh(const std::vector<Vertex> &vertices);

  Mesh mesh_;
  float width_ = 0, height_ = 0;
  Vec3 eye_{}, right_{}, up_{}, forward_{};
  std::vector<Vertex> strip_;
};

}  // namespace teapot

#endif  // CSIM_TEAPOT_SCENE_HPP
