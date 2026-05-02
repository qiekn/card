#pragma once

#include <optional>

#include <raylib.h>

#include "engine/atlas_registry.h"
#include "game/cardarea.h"
#include "layer.h"

// Owns the offscreen RenderTexture2D the game scene draws into, then displays
// it in a "Viewport" ImGui window. Also hosts placeholder Hierarchy and Console
// panels so the default dock layout has something in every region.
class GameLayer : public Layer {
 public:
  GameLayer();
  ~GameLayer() override;

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float dt) override;
  void OnRender() override;
  void OnImGuiRender() override;

  bool* ShowViewportPtr() { return &show_viewport_; }
  bool* ShowHierarchyPtr() { return &show_hierarchy_; }
  bool* ShowConsolePtr() { return &show_console_; }
  bool* ViewportNoTitleBarPtr() { return &viewport_no_titlebar_; }

  // Push the active scene background color (typically the ImGui theme's
  // background, sourced by Game::Render). Cheap to call every frame.
  void SetBackgroundColor(Color c) { background_color_ = c; }

  // Lets ImGuiLayer's Themes panel drive the texture-scale tier. Caller
  // writes 1 / 2 (/ 4 if you locally populate the dir); OnUpdate detects
  // the delta and reloads the atlas. If the target dir is missing, the
  // pointed-to value is reverted so the combo snaps back to the tier
  // that's actually on the GPU.
  int* TextureScalePtr() { return &texture_scale_; }

  // Exposed for Game::Render's "ImGui hidden = scene fullscreen" path.
  // When the chrome is hidden we resize the RT to window size (so the
  // blit is 1:1) and blit it directly to the backbuffer instead of
  // routing through ImGui::Image.
  void EnsureTargetSize(int w, int h) { EnsureTarget(w, h); }
  const RenderTexture2D& Target() const { return target_; }
  bool TargetValid() const { return target_valid_; }

  // Hand mouse arbitration (hover / click vs drag / release). Visible
  // mode calls it from DrawViewportPanel with ImGui's panel-relative
  // mouse + IsItemHovered; hidden mode calls it from Game::Render with
  // the raw ImGui mouse + hovered=true (RT covers the full backbuffer).
  void ProcessHandInput(Vector2 mouse_rt, bool hovered);

 private:
  void EnsureTarget(int w, int h);
  void DrawScene();
  void DrawViewportPanel();
  void DrawHierarchyPanel();
  void DrawConsolePanel();

  RenderTexture2D target_{};
  int target_w_ = 0;
  int target_h_ = 0;
  bool target_valid_ = false;

  bool show_viewport_ = true;
  bool show_hierarchy_ = true;
  bool show_console_ = true;
  bool viewport_no_titlebar_ = false;

  // Demo scene state — replace with real game state.
  float time_ = 0.0f;
  Color background_color_{30, 30, 46, 255};  // overridden per-frame by Game
  // Mirrors Balatro's G.SETTINGS.GRAPHICS.texture_scaling: picks which
  // assets/textures/{N}x/ subdir we sample. The Themes panel writes via
  // TextureScalePtr(); OnUpdate compares against atlases_.Tier() and
  // reloads on delta. If the target dir is missing the registry keeps
  // its previous tier and we revert this field to match. Default 2 —
  // what we ship in the repo. 1 / 4 work if you populate the dir.
  int texture_scale_ = 2;
  // Eager-loaded atlases from assets/atlases.json. Reload on tier change
  // is in-place (move-assign into existing slots), so the borrowed
  // Atlas* in each Card stays valid across the Themes panel combo flips.
  // The registry's Tier() is the source of truth for "what's on the GPU".
  engine::AtlasRegistry atlases_;
  // Phase 5 hand demo. std::optional because CardArea isn't default-
  // constructible (it needs viewport dims) and we build it in OnAttach.
  std::optional<game::CardArea> hand_;
  int next_sprite_idx_ = 0;  // walks through Joker atlas as cards are added

  // Drag/click arbitration state — see DrawViewportPanel. pressed_card_
  // is the card under the most recent mouse-down; if the mouse drags
  // past ImGui's threshold before release we hand off to CardArea's
  // drag controller, otherwise the release is treated as a click.
  game::Card* pressed_card_ = nullptr;
  Vector2 pressed_origin_rt_{};
};
