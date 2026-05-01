#pragma once

namespace engine {

// Transform — POD bag, mirrors Balatro's `Node.T` / `Movable.T` / `VT`.
// 6 fields: position (x,y), size (w,h), rotation r (radians), uniform scale.
// Game units, NOT pixels — pixel conversion lives at draw time.
//
// In its own namespace because raylib also defines a `Transform` (bone
// matrix for skinned animation, raylib.h ~451).
struct Transform {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
  float r = 0.0f;
  float scale = 1.0f;
};

}  // namespace engine
