#include "game_layer.h"

#include <imgui.h>

GameLayer::GameLayer() : Layer("GameLayer") {}

GameLayer::~GameLayer() = default;

void GameLayer::OnAttach() {
  // Allocate something non-zero so the first frame has a valid texture even
  // before the Viewport panel reports its real size.
  EnsureTarget(1280, 720);
}

void GameLayer::OnDetach() {
  if (target_valid_) {
    UnloadRenderTexture(target_);
    target_valid_ = false;
  }
}

void GameLayer::OnUpdate(float dt) { time_ += dt; }

void GameLayer::OnRender() {
  if (!target_valid_) return;
  DrawScene();
}

void GameLayer::OnImGuiRender() {
  if (show_viewport_) DrawViewportPanel();
  if (show_hierarchy_) DrawHierarchyPanel();
  if (show_console_) DrawConsolePanel();
}

void GameLayer::EnsureTarget(int w, int h) {
  if (w < 1 || h < 1) return;
  if (target_valid_ && target_w_ == w && target_h_ == h) return;
  if (target_valid_) UnloadRenderTexture(target_);
  target_ = LoadRenderTexture(w, h);
  SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
  target_w_ = w;
  target_h_ = h;
  target_valid_ = true;
}

void GameLayer::DrawScene() {
  BeginTextureMode(target_);
  ClearBackground(Color{30, 30, 46, 255});

  // Reference grid so resizes are visible.
  for (int x = 0; x < target_w_; x += 32) {
    DrawLine(x, 0, x, target_h_, Color{60, 60, 80, 255});
  }
  for (int y = 0; y < target_h_; y += 32) {
    DrawLine(0, y, target_w_, y, Color{60, 60, 80, 255});
  }

  // Spinning square at the centre — proves Update -> Render is wired.
  const float cx = static_cast<float>(target_w_) * 0.5f;
  const float cy = static_cast<float>(target_h_) * 0.5f;
  const float angle = time_ * 60.0f;
  Rectangle rect{cx, cy, 200, 200};
  DrawRectanglePro(rect, {100, 100}, angle, Color{220, 90, 90, 255});

  DrawText("Game Viewport", 16, 16, 24, RAYWHITE);
  DrawFPS(16, target_h_ - 30);

  EndTextureMode();
}

void GameLayer::DrawViewportPanel() {
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoScrollWithMouse;
  if (viewport_no_titlebar_) flags |= ImGuiWindowFlags_NoTitleBar;

  // Zero padding so the framebuffer fills the entire panel.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
  bool open = ImGui::Begin("Viewport", &show_viewport_, flags);
  ImGui::PopStyleVar();

  if (!open) {
    ImGui::End();
    return;
  }

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  EnsureTarget(static_cast<int>(avail.x), static_cast<int>(avail.y));

  if (target_valid_) {
    // raylib renders the FBO upside down relative to ImGui's UV convention,
    // so flip V.
    const ImTextureID tex_id = static_cast<ImTextureID>(target_.texture.id);
    ImGui::Image(tex_id, avail, ImVec2(0, 1), ImVec2(1, 0));
  }

  // Right-click anywhere in the viewport for the toggle — essential when the
  // title bar is hidden, since the menu bar route still works too. The Image
  // fills the panel so we must NOT pass NoOpenOverItems, otherwise the popup
  // never opens.
  if (ImGui::BeginPopupContextWindow("ViewportContext", ImGuiPopupFlags_MouseButtonRight)) {
    ImGui::MenuItem("Hide Title Bar", nullptr, &viewport_no_titlebar_);
    ImGui::EndPopup();
  }

  ImGui::End();
}

void GameLayer::DrawHierarchyPanel() {
  if (!ImGui::Begin("Hierarchy", &show_hierarchy_, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }
  if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BulletText("Camera");
    ImGui::BulletText("Player");
    ImGui::BulletText("World");
    ImGui::TreePop();
  }
  ImGui::End();
}

void GameLayer::DrawConsolePanel() {
  if (!ImGui::Begin("Console", &show_console_, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }
  ImGui::TextDisabled("[INFO] Game viewport initialized.");
  ImGui::TextDisabled("[INFO] Default dock layout applied.");
  ImGui::TextDisabled("[HINT] Right-click the viewport to toggle its title bar.");
  ImGui::End();
}
