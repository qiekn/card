#pragma once

#include <raylib.h>

#include "imgui_layer.h"
#include "layer_stack.h"

class GameLayer;

struct Game {
  void Run();

 private:
  void Init();

  void Tick();    // TimeStep / DeltaTime progress.
                  // Tick covers both game logic update and rendering.

  void Update();  // Update game logic.
  void Render();  // Render frame.

  void Shutdown();

  void ToggleBorderless();

 private:
  static constexpr int kScreenWidth = 1280;
  static constexpr int kScreenHeight = 720;
  static constexpr int kTargetFps = 144;

  LayerStack layers_;
  ImGuiLayer* imgui_layer_ = nullptr;  // non-owning; layers_ owns the unique_ptr.
  GameLayer* game_layer_ = nullptr;    // non-owning; layers_ owns the unique_ptr.

  bool borderless_ = false;
  Vector2 windowed_pos_{};
  Vector2 windowed_size_{};
};
