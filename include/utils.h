#pragma once

#include <raylib.h>
#include "object.h"

namespace util {
inline void DrawRectangleCenter(const Object& object, Color color = GREEN) {
  DrawRectangle(object.x, object.y, object.w, object.h, color);
}
}  // namespace util
