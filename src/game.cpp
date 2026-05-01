#include "game.h"

#include <raylib.h>
#include <rlgl.h>

#include <cstdio>
#include <memory>

#include "engine/text.h"
#include "game_layer.h"

namespace {
constexpr const char* kWindowStateFile = "window.state";

struct WindowState {
  int x;
  int y;
  int w;
  int h;
  int texture_scale;
};

WindowState LoadWindowState(int default_w, int default_h) {
  WindowState s{80, 80, default_w, default_h, 2};
  if (FILE* f = std::fopen(kWindowStateFile, "r")) {
    int x, y, w, h, ts;
    // Try the 5-field format first; fall back to legacy 4-field so older
    // window.state files parse without losing window pos.
    int n = std::fscanf(f, "%d %d %d %d %d", &x, &y, &w, &h, &ts);
    if (n >= 4 && w > 0 && h > 0) {
      s.x = x; s.y = y; s.w = w; s.h = h;
    }
    if (n == 5 && (ts == 1 || ts == 2)) {
      s.texture_scale = ts;
    }
    std::fclose(f);
  }
  return s;
}

void SaveWindowState(int texture_scale) {
  Vector2 pos = GetWindowPosition();
  if (FILE* f = std::fopen(kWindowStateFile, "w")) {
    std::fprintf(f, "%d %d %d %d %d\n", (int)pos.x, (int)pos.y,
                 GetScreenWidth(), GetScreenHeight(), texture_scale);
    std::fclose(f);
  }
}

void SetWindowIconFromPng(const char* path) {
  Image icon = LoadImage(path);
  if (icon.data != nullptr && icon.width > 0 && icon.height > 0) {
    SetWindowIcon(icon);
  }
  UnloadImage(icon);
}
}  // namespace

void Game::Run() {
  Init();

  while (!WindowShouldClose()) {
    Tick();
  }

  Shutdown();
}

void Game::Init() {
  SetTraceLogLevel(LOG_WARNING);
  const WindowState state = LoadWindowState(kScreenWidth, kScreenHeight);

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
  InitWindow(state.w, state.h, "card");
  SetWindowPosition(state.x, state.y);
  SetWindowIconFromPng("assets/icons/favicon.png");
  SetTargetFPS(kTargetFps);
  InitAudioDevice();

  // Pre-load OpenSans atlases so HUD text uses the same face as ImGui.
  // ASCII-only for now — switch to AsciiPlusCJK once we ship localized
  // strings (see engine/text.h).
  engine::LoadFonts(engine::CodepointSet::AsciiOnly);

  auto imgui_layer = std::make_unique<ImGuiLayer>();
  auto game_layer = std::make_unique<GameLayer>();

  imgui_layer_ = imgui_layer.get();
  game_layer_ = game_layer.get();

  // Restore the persisted tier before push_layer fires GameLayer::OnAttach,
  // so the very first atlas load picks the right dir (no init flicker).
  *game_layer->TextureScalePtr() = state.texture_scale;

  imgui_layer_->BindGamePanelToggles(game_layer->ShowViewportPtr(),
                                     game_layer->ShowHierarchyPtr(),
                                     game_layer->ShowConsolePtr(),
                                     game_layer->ViewportNoTitleBarPtr());
  imgui_layer_->BindTextureScale(game_layer->TextureScalePtr());

  // Order matters: ImGuiLayer must submit DockSpaceOverViewport before
  // GameLayer's Viewport window so the panel can dock into the central node
  // on the first frame.
  layers_.push_layer(std::move(imgui_layer));
  layers_.push_layer(std::move(game_layer));
}

void Game::Tick() {
  Update();
  Render();
}

void Game::Update() {
  const float dt = GetFrameTime();
  if (IsKeyPressed(KEY_F11)) {
    ToggleBorderless();
  }
  for (auto& layer : layers_) {
    layer->OnUpdate(dt);
  }
}

void Game::ToggleBorderless() {
  if (!borderless_) {
    windowed_pos_ = GetWindowPosition();
    windowed_size_ = {(float)GetScreenWidth(), (float)GetScreenHeight()};

    const int monitor = GetCurrentMonitor();
    const Vector2 mpos = GetMonitorPosition(monitor);
    const int mw = GetMonitorWidth(monitor);
    const int mh = GetMonitorHeight(monitor);

    SetWindowState(FLAG_WINDOW_UNDECORATED);
    SetWindowPosition((int)mpos.x, (int)mpos.y);
    // +1 px so Windows doesn't auto-promote this to exclusive fullscreen
    // (WS_POPUP + exact-monitor-size triggers fullscreen optimizations).
    SetWindowSize(mw, mh + 1);
    borderless_ = true;
  } else {
    ClearWindowState(FLAG_WINDOW_UNDECORATED);
    SetWindowSize((int)windowed_size_.x, (int)windowed_size_.y);
    SetWindowPosition((int)windowed_pos_.x, (int)windowed_pos_.y);
    borderless_ = false;
  }
}

void Game::Render() {
  // Sync the scene background to the active ImGui theme so the RT inside
  // the Viewport panel matches the surrounding chrome.
  game_layer_->SetBackgroundColor(imgui_layer_->BackgroundColor());

  BeginDrawing();
  ClearBackground(imgui_layer_->BackgroundColor());

  for (auto& layer : layers_) {
    layer->OnRender();
  }
  rlDrawRenderBatchActive();

  imgui_layer_->Begin();
  if (imgui_layer_->IsVisible()) {
    for (auto& layer : layers_) {
      layer->OnImGuiRender();
    }
  }
  imgui_layer_->End();

  EndDrawing();
}

void Game::Shutdown() {
  if (borderless_) ToggleBorderless();
  SaveWindowState(*game_layer_->TextureScalePtr());
  layers_.clear();  // detach layers before the GL context goes away
  engine::UnloadFonts();  // free font atlases (still need GL context)
  CloseAudioDevice();
  CloseWindow();
}
