#pragma once

#include <raylib.h>

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
};
