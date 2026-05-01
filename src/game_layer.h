#pragma once

#include <optional>

#include <raylib.h>

#include "engine/sprite.h"
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

 private:
  void EnsureTarget(int w, int h);
  void DrawScene();
  void DrawViewportPanel();
  void DrawHierarchyPanel();
  void DrawConsolePanel();

  // Try loading assets/textures/{tier}x/Jokers.png with cell sized to
  // (71*tier, 95*tier). On success replaces joker_atlas_ and returns
  // true; on failure leaves the current atlas alone. Atlas ctor already
  // wrote a stderr line on miss.
  bool TryLoadJokerAtlas(int tier);

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
  // TextureScalePtr(); OnUpdate observes the delta and reloads. Default
  // 2 — what we ship in the repo. 1 / 4 work if you populate the dir.
  int texture_scale_ = 2;
  // What tier is currently on the GPU. Stays 0 until first successful
  // load — that initial mismatch is what kicks OnAttach's load.
  int applied_texture_scale_ = 0;
  // Atlas first (Sprite holds a non-owning pointer into it). std::optional
  // because Sprite is move-only and constructed in OnAttach once Atlas is
  // loaded — outside of OnAttach the slot is empty.
  engine::Atlas joker_atlas_;
  std::optional<engine::Sprite> demo_;
  int demo_slot_ = 1;  // 1/2/3 keys map to T.x at 1/4, 1/2, 3/4 of viewport
};
