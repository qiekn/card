#pragma once

#include "engine/sprite.h"

namespace game {

// Card — MVP shell over engine::Sprite. Owns no atlas (passes the borrowed
// Atlas* through to Sprite); CardArea writes T.{x,y,r} every frame, the
// inherited Movable eases VT toward T, Sprite::Render draws one cell of
// the atlas at VT.
//
// Phase 6 grows this into the real lua Card: rank/suit, center (Joker /
// Tarot / enhancement), front+back+floating sprites, facing/flipping,
// CT for hit-testing during drag. For now it's literally a named Sprite
// plus a `highlighted_` flag (selection lift) so the demo wires CardArea
// against a concrete game type.
class Card : public engine::Sprite {
 public:
  Card(float x, float y, float w, float h, const engine::Atlas& atlas, int sprite_pos_x, int sprite_pos_y);

  bool Highlighted() const { return highlighted_; }
  void SetHighlighted(bool h) { highlighted_ = h; }

 private:
  bool highlighted_ = false;
};

}  // namespace game
