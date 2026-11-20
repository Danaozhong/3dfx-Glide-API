#include "scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <glide.h>

namespace teapot {

namespace {

constexpr float kPi = 3.14159265f;

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(float s, Vec3 v) { return {s * v.x, s * v.y, s * v.z}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 normalize(Vec3 v) { return (1.0f / std::sqrt(dot(v, v))) * v; }
Vec3 mix(Vec3 a, Vec3 b, float t) { return a + t * (b - a); }

Vec3 rotateY(Vec3 v, float c, float s) { return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z}; }

// Camera, looking at the teapot from the front, a bit from above.
const Vec3 kTarget{0.3f, 1.0f, 0.0f};
constexpr float kDistance = 10.0f;
constexpr float kElevation = 22.0f * kPi / 180.0f;
constexpr float kNear = 3.0f;  // depth range, in front of the camera
constexpr float kFar = 30.0f;

// Lights, directions toward the light.
const Vec3 kKeyLight = normalize(Vec3{-0.55f, 1.0f, 0.65f});
const Vec3 kFillLight = normalize(Vec3{0.9f, 0.25f, -0.4f});

// Colors, 0..1
const Vec3 kTeapotColor{0.78f, 0.10f, 0.08f};
const Vec3 kSkyTop{0.10f, 0.14f, 0.30f};
const Vec3 kSkyHorizon{0.62f, 0.68f, 0.80f};
const Vec3 kTileLight{0.86f, 0.86f, 0.88f};
const Vec3 kTileDark{0.42f, 0.44f, 0.50f};
const Vec3 kShadow{0.16f, 0.17f, 0.21f};

// Floor tiles of 1 unit. The floor reaches far back, where it fades into the
// sky, but not behind the camera: Glide does not clip triangles.
constexpr int kFloorLeft = -10, kFloorRight = 10;
constexpr int kFloorBack = -16, kFloorFront = 7;

}  // namespace

Scene::Scene(int subdivisions) : mesh_(tessellate(subdivisions)) {}

void Scene::setupState()
{
  grVertexLayout(GR_PARAM_XY, offsetof(Vertex, x), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_Z, offsetof(Vertex, z), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_RGB, offsetof(Vertex, r), GR_PARAM_ENABLE);
  grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_ITERATED,
                 GR_COMBINE_OTHER_NONE, FXFALSE);
  grCullMode(GR_CULL_DISABLE);
  grDepthBufferMode(GR_DEPTHBUFFER_ZBUFFER);
}

Scene::Vertex Scene::project(Vec3 p, Vec3 color) const
{
  const Vec3 q = p - eye_;
  const float depth = dot(q, forward_);
  const float focal = 1.25f * height_;
  Vertex v;
  v.x = 0.5f * width_ + focal * dot(q, right_) / depth;
  v.y = 0.5f * height_ - focal * dot(q, up_) / depth;
  v.z = 65535.0f * std::clamp((depth - kNear) / (kFar - kNear), 0.0f, 1.0f);
  v.r = 255.0f * std::min(color.x, 1.0f);
  v.g = 255.0f * std::min(color.y, 1.0f);
  v.b = 255.0f * std::min(color.z, 1.0f);
  return v;
}

// A gradient from the top down to the far edge of the floor, where the floor
// has faded to the horizon color.
void Scene::drawBackground()
{
  const float horizon = std::clamp(project(Vec3{0, 0, float(kFloorBack)}, kSkyHorizon).y, 0.0f, height_);
  auto vertex = [](float x, float y, Vec3 color) {
    return Vertex{x, y, 65535.0f, 255.0f * color.x, 255.0f * color.y, 255.0f * color.z};
  };
  const Vertex v[6] = {vertex(0, 0, kSkyTop),        vertex(width_, 0, kSkyTop),
                       vertex(0, horizon, kSkyHorizon), vertex(width_, horizon, kSkyHorizon),
                       vertex(0, height_, kSkyHorizon), vertex(width_, height_, kSkyHorizon)};
  grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 6, const_cast<Vertex *>(v), sizeof(Vertex));
}

void Scene::drawFloor()
{
  // Tiles fade into the sky toward the back.
  auto color = [](Vec3 tile, Vec3 p) {
    const float fade = std::clamp((-p.z - 2.0f) / (-kFloorBack - 4.0f), 0.0f, 1.0f);
    return mix(tile, kSkyHorizon, std::sqrt(fade));
  };
  for (int i = kFloorLeft; i < kFloorRight; i++) {
    for (int j = kFloorBack; j < kFloorFront; j++) {
      const Vec3 tile = ((i + j) & 1) ? kTileDark : kTileLight;
      const float x = float(i), z = float(j);
      const Vec3 corners[4] = {{x, 0, z}, {x + 1, 0, z}, {x, 0, z + 1}, {x + 1, 0, z + 1}};
      Vertex v[4];
      for (int k = 0; k < 4; k++) v[k] = project(corners[k], color(tile, corners[k]));
      grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 4, v, sizeof(Vertex));
    }
  }
}

// The mesh as one triangle strip per row of each patch.
void Scene::drawMesh(const std::vector<Vertex> &vertices)
{
  const int n = mesh_.subdivisions;
  for (int patch = 0; patch < mesh_.patches; patch++) {
    for (int row = 0; row < n; row++) {
      strip_.clear();
      for (int column = 0; column <= n; column++) {
        strip_.push_back(vertices[mesh_.index(patch, row, column)]);
        strip_.push_back(vertices[mesh_.index(patch, row + 1, column)]);
      }
      grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, static_cast<FxU32>(strip_.size()),
                                  strip_.data(), sizeof(Vertex));
    }
  }
}

void Scene::draw(float angle, float width, float height)
{
  width_ = width;
  height_ = height;
  eye_ = kTarget + kDistance * Vec3{0.0f, std::sin(kElevation), std::cos(kElevation)};
  forward_ = normalize(kTarget - eye_);
  right_ = normalize(cross(forward_, Vec3{0, 1, 0}));
  up_ = cross(right_, forward_);

  setupState();
  grDepthMask(FXTRUE);  // grBufferClear clears the depth buffer only with it
  grBufferClear(0, 0, 0xFFFF);

  // Sky and floor shadow do not write depth, the floor does.
  grDepthMask(FXFALSE);
  grDepthBufferFunction(GR_CMP_ALWAYS);
  drawBackground();
  grDepthMask(FXTRUE);
  drawFloor();

  const float c = std::cos(angle * kPi / 180.0f);
  const float s = std::sin(angle * kPi / 180.0f);
  std::vector<Vertex> vertices(mesh_.positions.size());

  // Shadow: the teapot flattened onto the floor along the key light.
  grDepthMask(FXFALSE);
  for (std::size_t i = 0; i < vertices.size(); i++) {
    const Vec3 p = rotateY(mesh_.positions[i], c, s);
    vertices[i] = project(p - (p.y / kKeyLight.y) * kKeyLight, kShadow);
  }
  drawMesh(vertices);

  // Teapot: ambient, diffuse key and fill light, and a white highlight.
  grDepthMask(FXTRUE);
  grDepthBufferFunction(GR_CMP_LESS);
  for (std::size_t i = 0; i < vertices.size(); i++) {
    const Vec3 p = rotateY(mesh_.positions[i], c, s);
    const Vec3 view = normalize(eye_ - p);
    Vec3 n = rotateY(mesh_.normals[i], c, s);
    if (dot(n, view) < 0) n = -1.0f * n;  // inside of the spout
    const float key = std::max(dot(n, kKeyLight), 0.0f);
    const float fill = std::max(dot(n, kFillLight), 0.0f);
    const float highlight = std::pow(std::max(dot(n, normalize(kKeyLight + view)), 0.0f), 60.0f);
    const Vec3 color = (0.12f + 0.85f * key + 0.30f * fill) * kTeapotColor + 0.9f * highlight * Vec3{1, 1, 1};
    vertices[i] = project(p, color);
  }
  drawMesh(vertices);
}

}  // namespace teapot
