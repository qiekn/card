#pragma once

#include <optional>

#include "transform.h"

namespace engine {

// Movable — base for any object that wants smooth motion.
//
// Two transforms: T is the target (set instantly by gameplay), VT is the
// visible one that exponentially eases toward T every frame. This split is
// Balatro's core trick — animation is implicit, callers never tween.
//
// Mirrors ref-balatro/engine/moveable.lua. MVP scope: only Major role
// (no Minor/Glued, no alignment string codes, no container hierarchy).
//
// Frame-rate independent: the ease coefficients are recomputed each Move
// from `exp(-K * dt)`, so the same K gives the same on-screen feel at
// 60 / 144 / 240 fps. K constants (50, 60, 190) lifted verbatim.

struct Velocity {
  float x = 0.0f;
  float y = 0.0f;
  float r = 0.0f;
  float scale = 0.0f;
};

struct Juice {
  // Set fresh each move_juice tick (additive into VT.scale / VT.r):
  float scale = 0.0f;
  float r = 0.0f;

  // Configured once at JuiceUp:
  float scale_amt = 0.0f;
  float r_amt = 0.0f;
  float start_time = 0.0f;
  float end_time = 0.0f;
};

class Movable {
 public:
  Movable() = default;
  Movable(float x, float y, float w, float h);
  virtual ~Movable() = default;

  Movable(const Movable&) = delete;
  Movable& operator=(const Movable&) = delete;
  Movable(Movable&&) = default;
  Movable& operator=(Movable&&) = default;

  // Per-frame ease step. Caller picks dt; we accumulate elapsed_ for juice
  // timing — no global clock dependency. Mirrors moveable.lua:302-355
  // (Major branch only).
  void Move(float dt);

  // Snap T to (x,y,w,h) and zero VT/velocity to match. Use during init or
  // teleports where ease would feel wrong. Mirrors moveable.lua:208.
  void HardSetT(float x, float y, float w, float h);

  // Snap VT.{x,y,w,h} to current T. Position only, leaves r/scale alone.
  // Mirrors moveable.lua:228.
  void HardSetVT();

  // Trigger a 0.4s squash-and-stretch. amount drives scale wobble; r_amt
  // drives rotation wobble (0 = none). Mirrors moveable.lua:268.
  void JuiceUp(float amount = 0.4f, float r_amt = 0.0f);

  // Hooks for derived classes (Sprite, Card, ...). Default no-op.
  virtual void Update(float /*dt*/) {}
  virtual void Render() {}

  Transform& T() { return t_; }
  const Transform& T() const { return t_; }
  Transform& VT() { return vt_; }
  const Transform& VT() const { return vt_; }

  bool IsStationary() const { return stationary_; }
  bool HasJuice() const { return juice_.has_value(); }

  // pinch.x = true makes VT.w shrink toward 0 (used by Card flip).
  void SetPinch(bool x, bool y) { pinch_x_ = x; pinch_y_ = y; }
  bool PinchX() const { return pinch_x_; }
  bool PinchY() const { return pinch_y_; }

 protected:
  Transform t_{};
  Transform vt_{};
  Velocity velocity_{};
  std::optional<Juice> juice_;

  bool pinch_x_ = false;
  bool pinch_y_ = false;
  float shadow_height_ = 0.2f;

  float elapsed_ = 0.0f;   // accumulated dt across Move calls
  bool stationary_ = true; // true if no axis is currently easing

 private:
  void MoveJuice();
  void MoveXY(float dt, float exp_xy, float max_vel);
  void MoveR(float dt, float exp_r);
  void MoveScale(float dt, float exp_scale);
  void MoveWH(float dt);
};

}  // namespace engine
