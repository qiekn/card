#pragma once

namespace engine::tuning {

// Live-tunable constants exposed through the ImGui Settings panel. These
// started as Balatro's lua "taste constants" (moveable.lua, cardarea.lua)
// — most are still anchored at their original values, but a handful were
// re-scaled where the lua game-unit semantics didn't survive the
// pixel-direct port (notably `max_vel_pps` — see comment on the field).
//
// Anyone can read/write these globals from any TU; ImGuiLayer's Settings
// panel mutates them via slider widgets. Once the feel is locked in we
// can promote them back to constexpr in their owning .cpp.

struct MovableEase {
  float exp_kxy = 50.0f;        // xy damping rate (higher = snappier landing)
  float exp_kscale = 60.0f;     // scale damping rate
  float exp_kr = 190.0f;        // r damping rate
  float xy_gain = 35.0f;        // (T - VT) -> velocity force gain
  // Velocity magnitude cap, in **pixels per second**. Lua's 70 was in
  // game-units/sec; multiplied by Balatro's tile scale (~32-64) it'd
  // be ~2240-4480 px/sec. We default to 5000 so snapback after a long
  // drag finishes in ~50 ms instead of ~2 s.
  float max_vel_pps = 5000.0f;
  float pinch_speed = 8.0f;     // mechanical pinch (flip animation)
  // Tilt-with-velocity coefficient (rad per unit vel.x). Lua's 0.015 was
  // in game-units; in our pixel-direct port one game-unit ≈ 30 px (a
  // typical Balatro tile), so dividing by ~30 lands near 0.0005 — but
  // playtesting at the real card display size (4× baseline = 284 px wide)
  // showed even 0.0005 spins too aggressively, 0.00015 is the sweet spot.
  float sway_coeff = 0.00015f;
};

struct HandLayout {
  float w_factor = 0.95f;          // hand width as fraction of viewport_w
  float max_w = 1600.0f;           // hard cap on hand width (wide viewports)
  float y_offset = 80.0f;          // px above viewport bottom
  float bow_factor = 0.4f;         // y-bow scale, as fraction of card_h
  float highlight_lift = 80.0f;    // px lift applied to highlighted cards
};

inline MovableEase ease;
inline HandLayout hand;

inline void ResetEase() { ease = MovableEase{}; }
inline void ResetHand() { hand = HandLayout{}; }

}  // namespace engine::tuning
