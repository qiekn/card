#include "game/cardarea.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "engine/tuning.h"

namespace game {

namespace {
bool HitRotatedRect(const engine::Transform& vt, Vector2 p) {
  // Translate p into the card's local frame (origin at center).
  const float cx = vt.x + vt.w * 0.5f;
  const float cy = vt.y + vt.h * 0.5f;
  const float dx = p.x - cx;
  const float dy = p.y - cy;
  // Inverse-rotate by -vt.r to get axis-aligned coords.
  const float c = std::cos(-vt.r);
  const float s = std::sin(-vt.r);
  const float lx = dx * c - dy * s;
  const float ly = dx * s + dy * c;
  // VT.scale shrinks/expands the visible rect uniformly (juice wobble).
  const float hw = vt.w * vt.scale * 0.5f;
  const float hh = vt.h * vt.scale * 0.5f;
  return std::fabs(lx) <= hw && std::fabs(ly) <= hh;
}
}  // namespace

CardArea::CardArea(float x, float y, float w, float h, CardAreaType type, float card_w, int temp_limit)
    : x_(x), y_(y), w_(w), h_(h), type_(type), card_w_(card_w), temp_limit_(temp_limit) {}

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
  if (dragged_ != nullptr) {
    // Drag controller writes the dragged card's T directly from the cursor
    // (AlignCards skipped it). r=0 so a held card reads as level — lua
    // does the same: dragged cards lose their fan tilt while in hand.
    const float tx = drag_mouse_.x - drag_offset_.x;
    const float ty = drag_mouse_.y - drag_offset_.y;
    dragged_->T().x = tx;
    dragged_->T().y = ty;
    dragged_->T().r = 0.0f;
    // Snap VT.{x,y} to T so the card is glued to the cursor — Movable's
    // ease constants (35*dt force, 50/s damping) are tuned for "settle
    // into slot", which lags ~30 ms and feels like swimming during drag.
    // r / scale stay eased so the grab still wobbles smoothly into level
    // (and juice continues to play out if the card was juicing).
    dragged_->VT().x = tx;
    dragged_->VT().y = ty;
    // Reorder by visual x so neighbors slide out of the way as the dragged
    // card crosses their slot center. stable_sort prevents jitter when two
    // cards share an x exactly. Next frame's AlignCards uses the new
    // indices to compute neighbors' new slots — they ease into place.
    std::stable_sort(cards_.begin(), cards_.end(),
                     [](const auto& a, const auto& b) { return a->T().x < b->T().x; });
  }
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
  // Draw non-dragged cards in slot order, then the dragged card last so
  // it floats above its neighbors during drag.
  for (auto& c : cards_) {
    if (c.get() != dragged_) c->Render();
  }
  if (dragged_ != nullptr) dragged_->Render();
}

Card* CardArea::FindHovered(Vector2 mouse) const {
  // Walk back-to-front so the visually topmost card wins (later cards in
  // the vector are drawn last → painted on top). MVP has no separate
  // hover-lift z-order, so paint order == hit-order is fine.
  for (auto it = cards_.rbegin(); it != cards_.rend(); ++it) {
    if (HitRotatedRect((*it)->VT(), mouse)) return it->get();
  }
  return nullptr;
}

void CardArea::StartDrag(Card* card, Vector2 mouse) {
  if (card == nullptr) return;
  dragged_ = card;
  drag_offset_.x = mouse.x - card->T().x;
  drag_offset_.y = mouse.y - card->T().y;
  drag_mouse_ = mouse;
}

void CardArea::UpdateDrag(Vector2 mouse) { drag_mouse_ = mouse; }

void CardArea::StopDrag() { dragged_ = nullptr; }

void CardArea::AlignCards(float t) {
  const int n = static_cast<int>(cards_.size());
  if (n == 0) return;

  const int M = std::max(n, temp_limit_);
  const float Mm1 = static_cast<float>(std::max(M - 1, 1));
  const float nf = static_cast<float>(n);

  for (int idx = 0; idx < n; ++idx) {
    const int k = idx + 1;  // 1-based to match lua formulas
    Card* c = cards_[idx].get();
    if (c == dragged_) continue;  // drag controller owns this card's T this frame
    const float kf = static_cast<float>(k);

    // Shared x slot — same shape for Hand and Play (cardarea.lua:692-722,
    // 787-810). The `(n - M)` correction centers the n cards inside the
    // area when n < temp_limit, so a shrinking hand collapses inward
    // instead of all hugging the left.
    const float lerp_x = (kf - 1.0f) / Mm1 - 0.5f * (nf - static_cast<float>(M)) / Mm1;
    const float slot_x = x_ + (w_ - card_w_) * lerp_x + 0.5f * (card_w_ - c->T().w);

    if (type_ == CardAreaType::Hand) {
      // Rotation arc: end cards lean ±0.1 rad outward (0.2 / 2). The sin
      // term uses card.T.x as phase so each card wobbles independently
      // — without it every card would tilt in lockstep and look fake.
      c->T().r = 0.2f * (-nf * 0.5f - 0.5f + kf) / nf + 0.02f * std::sin(2.0f * t + c->T().x);
      c->T().x = slot_x;

      // Vertical bow: |0..0.25| from end to center, doubled to give the
      // hand its "lifted middle" silhouette. The lua's "+ abs(...) - 0.2"
      // is in game units; we lift it into pixel space by * card_h. The
      // sign on `bow` and the constant offset are tuned to keep the area
      // rect as the visual baseline (top of cards aligns with y_).
      const float bow = std::fabs(0.5f * (-nf * 0.5f + kf - 0.5f) / nf);  // 0..0.25
      // Highlighted cards lift up — mirrors lua's `- highlight_height`
      // term in the y formula. Click toggles the flag in GameLayer.
      const auto& tune = engine::tuning::hand;
      const float lift = c->Highlighted() ? tune.highlight_lift : 0.0f;
      c->T().y = y_ + h_ * 0.5f - c->T().h * 0.5f - lift - bow * c->T().h * tune.bow_factor +
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
