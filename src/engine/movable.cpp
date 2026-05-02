#include "engine/movable.h"

#include <algorithm>
#include <cmath>

#include "engine/tuning.h"

namespace engine {

namespace {

// Snap thresholds: stop easing when both error and velocity are small.
// Kept compile-time — these aren't taste constants, they're floating-point
// hygiene to stop spurious oscillation around T.
constexpr float kSnapXY = 0.01f;
constexpr float kSnapR = 0.001f;
constexpr float kSnapScale = 0.001f;

// move_juice frequencies (rad/sec) and end-of-life envelope exponents.
// Live-tuning juice would only confuse playtesting; promote later if
// needed.
constexpr float kJuiceScaleFreq = 50.8f;
constexpr float kJuiceRFreq = 40.8f;
constexpr float kJuiceDuration = 0.4f;       // seconds
constexpr float kJuiceImmediateSquash = 0.6f;  // VT.scale = 1 - 0.6 * amount

}  // namespace

Movable::Movable(float x, float y, float w, float h) {
  HardSetT(x, y, w, h);
}

void Movable::HardSetT(float x, float y, float w, float h) {
  t_.x = x;
  t_.y = y;
  t_.w = w;
  t_.h = h;

  velocity_ = {};

  vt_.x = x;
  vt_.y = y;
  vt_.w = w;
  vt_.h = h;
  vt_.r = t_.r;
  vt_.scale = t_.scale;
}

void Movable::HardSetVT() {
  vt_.x = t_.x;
  vt_.y = t_.y;
  vt_.w = t_.w;
  vt_.h = t_.h;
}

void Movable::JuiceUp(float amount, float r_amt) {
  Juice j{};
  j.scale = 0.0f;
  j.scale_amt = amount;
  j.r = 0.0f;
  j.r_amt = r_amt;
  j.start_time = elapsed_;
  j.end_time = elapsed_ + kJuiceDuration;
  juice_ = j;

  // Immediate squash: VT.scale snaps below 1 so the sin wobble starts
  // from a stretched state and bounces back — feels more "punchy" than
  // wobbling around 1.0. Lua: VT.scale = 1 - 0.6 * amount.
  vt_.scale = 1.0f - kJuiceImmediateSquash * amount;
}

void Movable::Move(float dt) {
  elapsed_ += dt;

  const auto& e = engine::tuning::ease;
  const float exp_xy = std::exp(-e.exp_kxy * dt);
  const float exp_scale = std::exp(-e.exp_kscale * dt);
  const float exp_r = std::exp(-e.exp_kr * dt);
  const float max_vel = e.max_vel_pps * dt;

  stationary_ = true;

  MoveJuice();
  MoveXY(dt, exp_xy, max_vel);
  MoveR(dt, exp_r);
  MoveScale(dt, exp_scale);
  MoveWH(dt);
}

void Movable::MoveJuice() {
  if (!juice_) return;
  if (juice_->end_time < elapsed_) {
    juice_.reset();
    return;
  }
  const float t = elapsed_ - juice_->start_time;
  const float duration = juice_->end_time - juice_->start_time;
  const float fade = std::max(0.0f, (juice_->end_time - elapsed_) / duration);

  // scale envelope: sin * fade^3 (cubic falloff, sharper landing).
  juice_->scale = juice_->scale_amt * std::sin(kJuiceScaleFreq * t) * fade * fade * fade;
  // r envelope: sin * fade^2 (quadratic — softer rotation tail).
  juice_->r = juice_->r_amt * std::sin(kJuiceRFreq * t) * fade * fade;
}

void Movable::MoveXY(float dt, float exp_xy, float max_vel) {
  const bool need_x = (t_.x != vt_.x) || (std::abs(velocity_.x) > kSnapXY);
  const bool need_y = (t_.y != vt_.y) || (std::abs(velocity_.y) > kSnapXY);
  if (!need_x && !need_y) return;

  const float xy_gain = engine::tuning::ease.xy_gain;
  velocity_.x = exp_xy * velocity_.x + (1.0f - exp_xy) * (t_.x - vt_.x) * xy_gain * dt;
  velocity_.y = exp_xy * velocity_.y + (1.0f - exp_xy) * (t_.y - vt_.y) * xy_gain * dt;

  // Clamp velocity magnitude — prevents distant T jumps from launching VT
  // across the screen in one frame. Pure 2D vector clamp.
  const float mag2 = velocity_.x * velocity_.x + velocity_.y * velocity_.y;
  if (mag2 > max_vel * max_vel) {
    const float mag = std::sqrt(mag2);
    velocity_.x = max_vel * velocity_.x / mag;
    velocity_.y = max_vel * velocity_.y / mag;
  }

  stationary_ = false;
  vt_.x += velocity_.x;
  vt_.y += velocity_.y;

  if (std::abs(vt_.x - t_.x) < kSnapXY && std::abs(velocity_.x) < kSnapXY) {
    vt_.x = t_.x;
    velocity_.x = 0.0f;
  }
  if (std::abs(vt_.y - t_.y) < kSnapXY && std::abs(velocity_.y) < kSnapXY) {
    vt_.y = t_.y;
    velocity_.y = 0.0f;
  }
}

void Movable::MoveScale(float dt, float exp_scale) {
  const float juice_scale = juice_ ? juice_->scale : 0.0f;
  const float des_scale = t_.scale + juice_scale;

  if (des_scale == vt_.scale && std::abs(velocity_.scale) <= kSnapScale) {
    return;
  }
  stationary_ = false;
  velocity_.scale = exp_scale * velocity_.scale + (1.0f - exp_scale) * (des_scale - vt_.scale);
  vt_.scale += velocity_.scale;
}

void Movable::MoveR(float dt, float exp_r) {
  // Side sway: 0.015 * vel.x / dt makes the object tilt as it slides.
  // dt > 0 always here (we accumulate elapsed_ before calling).
  const float juice_r = juice_ ? juice_->r * 2.0f : 0.0f;
  const float sway_coeff = engine::tuning::ease.sway_coeff;
  const float sway = (dt > 0.0f) ? (sway_coeff * velocity_.x / dt) : 0.0f;
  const float des_r = t_.r + sway + juice_r;

  if (des_r != vt_.r || std::abs(velocity_.r) > kSnapR) {
    stationary_ = false;
    velocity_.r = exp_r * velocity_.r + (1.0f - exp_r) * (des_r - vt_.r);
    vt_.r += velocity_.r;
  }
  if (std::abs(vt_.r - t_.r) < kSnapR && std::abs(velocity_.r) < kSnapR) {
    vt_.r = t_.r;
    velocity_.r = 0.0f;
  }
}

void Movable::MoveWH(float dt) {
  // pinch.x true: ease VT.w toward 0; false: ease toward T.w.
  // Constant velocity (8 * dt * T.w/h), not exp ease — pinch needs to feel
  // mechanical (flip animation) not springy.
  const bool need_w = (t_.w != vt_.w && !pinch_x_) || (vt_.w > 0.0f && pinch_x_);
  const bool need_h = (t_.h != vt_.h && !pinch_y_) || (vt_.h > 0.0f && pinch_y_);
  if (!need_w && !need_h) return;

  const float pinch_speed = engine::tuning::ease.pinch_speed;
  stationary_ = false;
  vt_.w += pinch_speed * dt * (pinch_x_ ? -1.0f : 1.0f) * t_.w;
  vt_.h += pinch_speed * dt * (pinch_y_ ? -1.0f : 1.0f) * t_.h;
  vt_.w = std::clamp(vt_.w, 0.0f, t_.w);
  vt_.h = std::clamp(vt_.h, 0.0f, t_.h);
}

}  // namespace engine
