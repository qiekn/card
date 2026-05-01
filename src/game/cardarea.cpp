#include "game/cardarea.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {

CardArea::CardArea(float x, float y, float w, float h, CardAreaType type,
                   float card_w, int temp_limit)
    : x_(x),
      y_(y),
      w_(w),
      h_(h),
      type_(type),
      card_w_(card_w),
      temp_limit_(temp_limit) {}

Card* CardArea::Emplace(std::unique_ptr<Card> card) {
  Card* raw = card.get();
  cards_.push_back(std::move(card));
  return raw;
}

std::unique_ptr<Card> CardArea::RemoveBack() {
  if (cards_.empty()) return nullptr;
  std::unique_ptr<Card> out = std::move(cards_.back());
  cards_.pop_back();
  return out;
}

void CardArea::Tick(float dt, float real_time) {
  AlignCards(real_time);
  for (auto& c : cards_) c->Move(dt);
}

void CardArea::HardSetCards(float real_time) {
  AlignCards(real_time);
  for (auto& c : cards_) {
    const engine::Transform& t = c->T();
    // HardSetT writes T's xywh, zeros velocity, and snaps VT.{x,y,w,h,r,scale}
    // to T — exactly what we want for "card already in slot, no fly-in".
    c->HardSetT(t.x, t.y, t.w, t.h);
  }
}

void CardArea::SetBounds(float x, float y, float w, float h) {
  x_ = x;
  y_ = y;
  w_ = w;
  h_ = h;
}

void CardArea::Render() {
  for (auto& c : cards_) c->Render();
}

void CardArea::AlignCards(float t) {
  const int n = static_cast<int>(cards_.size());
  if (n == 0) return;

  const int M = std::max(n, temp_limit_);
  const float Mm1 = static_cast<float>(std::max(M - 1, 1));
  const float nf = static_cast<float>(n);

  for (int idx = 0; idx < n; ++idx) {
    const int k = idx + 1;  // 1-based to match lua formulas
    Card* c = cards_[idx].get();
    const float kf = static_cast<float>(k);

    // Shared x slot — same shape for Hand and Play (cardarea.lua:692-722,
    // 787-810). The `(n - M)` correction centers the n cards inside the
    // area when n < temp_limit, so a shrinking hand collapses inward
    // instead of all hugging the left.
    const float lerp_x =
        (kf - 1.0f) / Mm1 - 0.5f * (nf - static_cast<float>(M)) / Mm1;
    const float slot_x =
        x_ + (w_ - card_w_) * lerp_x + 0.5f * (card_w_ - c->T().w);

    if (type_ == CardAreaType::Hand) {
      // Rotation arc: end cards lean ±0.1 rad outward (0.2 / 2). The sin
      // term uses card.T.x as phase so each card wobbles independently
      // — without it every card would tilt in lockstep and look fake.
      c->T().r = 0.2f * (-nf * 0.5f - 0.5f + kf) / nf +
                 0.02f * std::sin(2.0f * t + c->T().x);
      c->T().x = slot_x;

      // Vertical bow: |0..0.25| from end to center, doubled to give the
      // hand its "lifted middle" silhouette. The lua's "+ abs(...) - 0.2"
      // is in game units; we lift it into pixel space by * card_h. The
      // sign on `bow` and the constant offset are tuned to keep the area
      // rect as the visual baseline (top of cards aligns with y_).
      const float bow =
          std::fabs(0.5f * (-nf * 0.5f + kf - 0.5f) / nf);  // 0..0.25
      c->T().y = y_ + h_ * 0.5f - c->T().h * 0.5f -
                 bow * c->T().h * 0.4f +
                 0.03f * c->T().h * std::sin(0.666f * t + c->T().x);
    } else {
      // Play: flat row, no rotation, no wobble.
      c->T().r = 0.0f;
      c->T().x = slot_x;
      c->T().y = y_ + h_ * 0.5f - c->T().h * 0.5f;
    }
  }
}

}  // namespace game
