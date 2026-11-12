// Interior cells: furnished rooms (cottage, hall, chapel) built as static meshes in a hidden corner of the map, high above the world.
#pragma once
namespace xi {
constexpr float BASEY = 900.f;
struct Circ { float x, z, r; };
struct Def { const char* name; float cx, cz, hx, hz, h; int nc; Circ c[20]; float chestX, chestZ, bedX, bedZ, npcX, npcZ; };
// local coordinates: floor at y = 0, door in the +z wall at x = 0, player enters at (0, hz - 1.4) looking toward -z
inline const Def DEFS[3] = {
  {"COTTAGE", -960.f, -960.f, 4.4f, 3.5f, 3.1f, 9, {{-3.15f, -2.4f, 1.2f}, {0.f, -2.95f, 1.1f}, {1.7f, 0.2f, 1.05f}, {0.5f, 0.1f, 0.35f}, {2.9f, 0.5f, 0.35f}, {3.95f, -1.6f, 0.75f}, {3.95f, -0.4f, 0.55f}, {-3.3f, 1.9f, 0.7f}, {3.6f, 2.6f, 0.55f}}, -3.3f, 1.9f, -3.15f, -2.4f, 1.0f, 0.9f},
  {"GREAT HALL", -880.f, -960.f, 7.5f, 5.2f, 4.8f, 12, {{0.f, -4.5f, 1.8f}, {-3.f, -0.2f, 1.1f}, {-3.f, 1.6f, 1.1f}, {-3.f, -2.0f, 1.1f}, {3.f, -0.2f, 1.1f}, {3.f, 1.6f, 1.1f}, {3.f, -2.0f, 1.1f}, {-6.6f, -3.8f, 0.7f}, {6.6f, -3.8f, 0.7f}, {-6.6f, 3.8f, 0.8f}, {6.6f, 3.8f, 0.8f}, {0.f, -2.3f, 0.9f}}, 6.6f, -3.8f, -6.6f, 3.8f, 0.f, 0.f},
  {"CHAPEL", -800.f, -960.f, 5.2f, 8.2f, 7.4f, 12, {{0.f, -6.6f, 1.3f}, {-3.4f, -4.f, 0.5f}, {3.4f, -4.f, 0.5f}, {-3.4f, 0.f, 0.5f}, {3.4f, 0.f, 0.5f}, {-3.4f, 4.f, 0.5f}, {3.4f, 4.f, 0.5f}, {-1.9f, -1.6f, 1.0f}, {1.9f, -1.6f, 1.0f}, {-1.9f, 1.0f, 1.0f}, {1.9f, 1.0f, 1.0f}, {-1.9f, 3.6f, 1.0f}}, 4.4f, 6.6f, 0.f, 0.f, 0.f, -3.f}};
}
