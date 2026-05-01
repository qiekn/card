#pragma once

// Transform — POD bag, mirrors Balatro's `Node.T` / `Movable.T` / `VT`.
// 6 fields: position (x,y), size (w,h), rotation r (radians), uniform scale.
// Game units, NOT pixels — pixel conversion lives at draw time.
struct Transform {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
  float r = 0.0f;
  float scale = 1.0f;
};
