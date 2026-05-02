#pragma once

#include <memory>
#include <vector>

#include <raylib.h>

#include "game/card.h"

namespace game {

// CardArea types we ship in MVP. Each maps to one branch of the lua
// `align_cards` switch (cardarea.lua:615-883). Phase 5 covers Hand and
// Play; the rest (Joker / consumeable / shop / deck / discard / voucher)
// land alongside their respective gameplay phases.
enum class CardAreaType {
  Hand,
  Play,
};

// CardArea — owns a list of Cards and lays them out every frame against
// the type's formula. Mirrors ref-balatro/cardarea.lua, MVP scope:
//
//   - Hand and Play formulas only (§2.1, §2.2 in 02 notes).
//   - Owns its cards via unique_ptr (lua keeps cards in G.I.CARD and
//     CardArea.cards is just a non-owning view; we collapse that for now
//     since cards never transfer between areas in the demo).
//   - No drag / hover / highlight / sort yet — those land with input
//     handling in Phase 6+.
//   - The area itself is a static rectangle in pixel space, not a
//     Movable. Promote when juicing the area (e.g. shake on bust)
//     becomes a thing.
//
// Tick order matches lua: `align_cards` writes T on every card, then
// each card's Move() eases VT toward the new T. Calling Render() right
// after draws everything in slot order (left-to-right).
class CardArea {
 public:
  CardArea(float x, float y, float w, float h, CardAreaType type,
           float card_w, int temp_limit);

  // Adds card to the back of the slot list and returns a non-owning
  // pointer so the caller can keep tabs (juice / etc).
  Card* Emplace(std::unique_ptr<Card> card);

  // Pops the rightmost card and returns ownership. Null if empty.
  std::unique_ptr<Card> RemoveBack();

  // Per-frame: rewrite each card's T from the area formula and ease.
  // `real_time` feeds the per-card sin idle wobble (decoupled from dt
  // so pause / slow-mo can run on `dt` without freezing the wobble).
  void Tick(float dt, float real_time);

  // Snap every card's VT to its slot T, skipping the ease. Use after the
  // initial deal so cards don't fly in from their spawn position on the
  // very first frame; mirrors lua's `hard_set_cards` (cardarea.lua:902).
  void HardSetCards(float real_time);

  // Resets the area's pixel rect. Cheap to call every frame — formula
  // re-reads the rect each Tick.
  void SetBounds(float x, float y, float w, float h);

  // Draws all owned cards in slot order.
  void Render();

  // Hit-tests `mouse` (in RT/scene pixel coords) against each card's VT,
  // walking back-to-front so the visually topmost card wins. Returns
  // nullptr if no card is hit.
  Card* FindHovered(Vector2 mouse) const;

  size_t Size() const { return cards_.size(); }
  Card* At(size_t i) { return cards_[i].get(); }

  float X() const { return x_; }
  float Y() const { return y_; }
  float W() const { return w_; }
  float H() const { return h_; }

 private:
  void AlignCards(float real_time);

  float x_ = 0.0f;
  float y_ = 0.0f;
  float w_ = 0.0f;
  float h_ = 0.0f;

  CardAreaType type_ = CardAreaType::Hand;
  float card_w_ = 0.0f;     // canonical slot width (≠ card.T.w in MVP they match)
  int temp_limit_ = 8;      // reserves slot-room when n < temp_limit (hand)

  std::vector<std::unique_ptr<Card>> cards_;
};

}  // namespace game
