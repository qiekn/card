#include "engine/sprite.h"

#include <cstdio>

namespace engine {

Atlas::Atlas(const char* path, int px, int py) : px_(px), py_(py) {
  texture_ = LoadTexture(path);
  if (texture_.id == 0) {
    std::fprintf(stderr,
                 "[atlas] failed to load %s — did you run "
                 "tools/update-balatro-assets.sh? See README.\n",
                 path);
    return;
  }
  // POINT keeps Balatro's pixel art crisp at any draw-time scale; bilinear
  // would smear the 1px outlines that frame every card.
  SetTextureFilter(texture_, TEXTURE_FILTER_POINT);
}

Atlas::~Atlas() {
  if (texture_.id != 0) UnloadTexture(texture_);
}

Atlas::Atlas(Atlas&& other) noexcept
    : texture_(other.texture_), px_(other.px_), py_(other.py_) {
  other.texture_ = {};
  other.px_ = 0;
  other.py_ = 0;
}

Atlas& Atlas::operator=(Atlas&& other) noexcept {
  if (this != &other) {
    if (texture_.id != 0) UnloadTexture(texture_);
    texture_ = other.texture_;
    px_ = other.px_;
    py_ = other.py_;
    other.texture_ = {};
    other.px_ = 0;
    other.py_ = 0;
  }
  return *this;
}

Sprite::Sprite(float x, float y, float w, float h, const Atlas& atlas,
               int sprite_pos_x, int sprite_pos_y)
    : Movable(x, y, w, h),
      atlas_(&atlas),
      sprite_x_(sprite_pos_x),
      sprite_y_(sprite_pos_y) {}

void Sprite::Render() {
  if (atlas_ == nullptr || !atlas_->Loaded()) return;

  const Transform& vt = VT();
  const float draw_w = vt.w * vt.scale;
  const float draw_h = vt.h * vt.scale;

  // src: pluck the (sprite_x_, sprite_y_) cell out of the atlas grid.
  // Mirrors sprite.lua:25-43 set_sprite_pos's love.graphics.newQuad call.
  const Rectangle src{
      static_cast<float>(sprite_x_ * atlas_->CellPx()),
      static_cast<float>(sprite_y_ * atlas_->CellPy()),
      static_cast<float>(atlas_->CellPx()),
      static_cast<float>(atlas_->CellPy()),
  };

  // dst: VT-driven rectangle, rotation pivots around its center. This is
  // sprite.lua:158-190 draw_self compressed into one DrawTexturePro:
  // raylib's `origin` arg replaces lua's translate-rotate-translate dance.
  const Rectangle dst{vt.x + vt.w * 0.5f, vt.y + vt.h * 0.5f, draw_w, draw_h};
  const Vector2 origin{draw_w * 0.5f, draw_h * 0.5f};
  const float deg = vt.r * 57.2957795f;  // raylib wants degrees

  DrawTexturePro(atlas_->Texture(), src, dst, origin, deg, WHITE);
}

}  // namespace engine
