#include "game/card.h"

namespace game {

Card::Card(float x, float y, float w, float h, const engine::Atlas& atlas, int sprite_pos_x, int sprite_pos_y)
    : engine::Sprite(x, y, w, h, atlas, sprite_pos_x, sprite_pos_y) {}

}  // namespace game
