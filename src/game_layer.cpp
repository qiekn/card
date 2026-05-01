#include "game_layer.h"

#include <imgui.h>

GameLayer::GameLayer() : Layer("GameLayer") {}

GameLayer::~GameLayer() = default;

void GameLayer::OnAttach() {
  // Allocate something non-zero so the first frame has a valid texture even
  // before the Viewport panel reports its real size.
  EnsureTarget(1280, 720);

  // Movable demo: a 200x200 box that eases between three slots.
  // T.x set instantly by 1/2/3 keys; VT.x exp-eases each frame.
  const float w = 200.0f;
  const float h = 200.0f;
  const float center_x = static_cast<float>(target_w_) * 0.5f - w * 0.5f;
  const float center_y = static_cast<float>(target_h_) * 0.5f - h * 0.5f;
  demo_.HardSetT(center_x, center_y, w, h);
}

void GameLayer::OnDetach() {
  if (target_valid_) {
    UnloadRenderTexture(target_);
    target_valid_ = false;
  }
}

void GameLayer::OnUpdate(float dt) {
  time_ += dt;

  // Demo controls — 1/2/3 set T.x to 1/4, 1/2, 3/4 of viewport.
  // VT.x exp-eases toward T.x via demo_.Move(dt) below.
  const float w = demo_.T().w;
  const float y = static_cast<float>(target_h_) * 0.5f - demo_.T().h * 0.5f;
  const float vw = static_cast<float>(target_w_);
  if (IsKeyPressed(KEY_ONE))   { demo_.T().x = vw * 0.25f - w * 0.5f; demo_slot_ = 1; }
  if (IsKeyPressed(KEY_TWO))   { demo_.T().x = vw * 0.50f - w * 0.5f; demo_slot_ = 2; }
  if (IsKeyPressed(KEY_THREE)) { demo_.T().x = vw * 0.75f - w * 0.5f; demo_slot_ = 3; }
  // Re-anchor y in case the viewport was resized.
  demo_.T().y = y;

  if (IsKeyPressed(KEY_J)) demo_.JuiceUp(0.4f, 0.0f);

  demo_.Move(dt);
}

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

  // Three slot markers (faint) so you can see where 1/2/3 send the box.
  const float vw = static_cast<float>(target_w_);
  for (int slot = 1; slot <= 3; ++slot) {
    const float fx = vw * 0.25f * static_cast<float>(slot);
    DrawLine(static_cast<int>(fx), 0, static_cast<int>(fx), target_h_,
             Color{80, 80, 110, 255});
  }

  // The Movable box: position+size from VT (eased), centered rotation.
  const auto& vt = demo_.VT();
  const float draw_w = vt.w * vt.scale;
  const float draw_h = vt.h * vt.scale;
  Rectangle rect{vt.x + vt.w * 0.5f, vt.y + vt.h * 0.5f, draw_w, draw_h};
  Vector2 origin{draw_w * 0.5f, draw_h * 0.5f};
  const float deg = vt.r * 57.2957795f;  // raylib wants degrees
  DrawRectanglePro(rect, origin, deg, Color{220, 90, 90, 255});

  DrawText(TextFormat("Slot %d  (1/2/3 to move, J to juice)", demo_slot_),
           16, 16, 20, RAYWHITE);
  DrawText(TextFormat("T.x=%.1f  VT.x=%.1f  juice=%s",
                      demo_.T().x, demo_.VT().x,
                      demo_.HasJuice() ? "yes" : "no"),
           16, 42, 16, Color{180, 180, 200, 255});
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
