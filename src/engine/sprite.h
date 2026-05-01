#pragma once

#include <raylib.h>

#include "engine/movable.h"

namespace engine {

// Atlas — RAII wrapper around a raylib Texture2D + Balatro's per-cell
// metadata (px, py). Mirrors the `atlas` table in ref-balatro game.lua:5660+
// — one row of which is `{ name, image, px, py }`.
//
// `px,py` is the **per-cell pixel size** (e.g. 71x95 for cards / Jokers /
// Tarots / Vouchers / Boosters / Enhancers / stickers). Sprite multiplies
// `sprite_pos.{x,y}` by these to slice the source rectangle.
//
// Move-only. The dtor calls UnloadTexture, so dropping an Atlas releases
// its GPU memory; moving from one transfers ownership and zeroes the
// source. Loading the same path twice loads twice — there's no cache here,
// callers de-dupe at the registry level.
class Atlas {
 public:
  Atlas() = default;

  // Loads the texture from `path` (resolved relative to CWD, same as the
  // text module). Sets TEXTURE_FILTER_POINT so atlas-grade pixel art stays
  // crisp under any draw-time scale. On load failure prints a hint to
  // stderr and leaves Loaded() == false; the caller decides whether to
  // assert / fall back.
  Atlas(const char* path, int px, int py);

  ~Atlas();

  Atlas(const Atlas&) = delete;
  Atlas& operator=(const Atlas&) = delete;
  Atlas(Atlas&& other) noexcept;
  Atlas& operator=(Atlas&& other) noexcept;

  bool Loaded() const { return texture_.id != 0; }
  const Texture2D& Texture() const { return texture_; }
  int CellPx() const { return px_; }
  int CellPy() const { return py_; }

 private:
  Texture2D texture_{};  // .id == 0 means empty / moved-from
  int px_ = 0;
  int py_ = 0;
};

// Sprite — a Movable that draws one cell of an Atlas. Mirrors
// ref-balatro/engine/sprite.lua, MVP scope only:
//   - no shaders / draw_steps / shadow path     (Phase 7+)
//   - no animation_atli (`sprite_pos.v` random) (Phase 7+)
//   - no `draw_from` (borrowed transform)       (Phase 5+ if needed)
//   - no `image_dims` / `scale_mag` fields      (shader uniforms only)
//
// Render() pulls VT from Movable, builds a (src, dst) pair and hands the
// whole thing to raylib's DrawTexturePro. T.{w,h} are pixels — we are NOT
// running through Balatro's `G.TILESCALE * G.TILESIZE` step yet; that
// abstraction enters when CardArea needs game-unit math.
class Sprite : public Movable {
 public:
  Sprite(float x, float y, float w, float h, const Atlas& atlas,
         int sprite_pos_x, int sprite_pos_y);

  void Render() override;

  void SetSpritePos(int x, int y) { sprite_x_ = x; sprite_y_ = y; }
  int SpritePosX() const { return sprite_x_; }
  int SpritePosY() const { return sprite_y_; }

 private:
  const Atlas* atlas_ = nullptr;  // non-owning; outlive the Sprite.
  int sprite_x_ = 0;
  int sprite_y_ = 0;
};

}  // namespace engine
