#include "game_layer.h"

#include <imgui.h>

#include "engine/text.h"

GameLayer::GameLayer() : Layer("GameLayer") {}

GameLayer::~GameLayer() = default;

void GameLayer::OnAttach() {
  // Allocate something non-zero so the first frame has a valid texture even
  // before the Viewport panel reports its real size.
  EnsureTarget(1280, 720);

  // Load the Joker atlas. 71x95 per cell — Balatro's universal card size.
  // Path is relative to CWD (project root in dev), same convention as the
  // text module's font paths.
  joker_atlas_ = engine::Atlas{"assets/balatro/textures/1x/Jokers.png", 71, 95};

  // Sprite demo: the vanilla Joker (sprite_pos {0,0}) at 4x cell size so it
  // reads at typical viewport sizes. T.x set instantly by 1/2/3 keys; VT.x
  // exp-eases each frame via Movable::Move.
  constexpr float kCardScale = 4.0f;
  const float w = 71.0f * kCardScale;
  const float h = 95.0f * kCardScale;
  const float center_x = static_cast<float>(target_w_) * 0.5f - w * 0.5f;
  const float center_y = static_cast<float>(target_h_) * 0.5f - h * 0.5f;
  demo_.emplace(center_x, center_y, w, h, joker_atlas_, /*sprite_x=*/0,
                /*sprite_y=*/0);
}

void GameLayer::OnDetach() {
  if (target_valid_) {
    UnloadRenderTexture(target_);
    target_valid_ = false;
  }
  demo_.reset();
  joker_atlas_ = {};
}

void GameLayer::OnUpdate(float dt) {
  time_ += dt;

  if (!demo_) return;

  // Demo controls — 1/2/3 set T.x to 1/4, 1/2, 3/4 of viewport.
  // VT.x exp-eases toward T.x via demo_->Move(dt) below.
  const float w = demo_->T().w;
  const float y = static_cast<float>(target_h_) * 0.5f - demo_->T().h * 0.5f;
  const float vw = static_cast<float>(target_w_);
  if (IsKeyPressed(KEY_ONE))   { demo_->T().x = vw * 0.25f - w * 0.5f; demo_slot_ = 1; }
  if (IsKeyPressed(KEY_TWO))   { demo_->T().x = vw * 0.50f - w * 0.5f; demo_slot_ = 2; }
  if (IsKeyPressed(KEY_THREE)) { demo_->T().x = vw * 0.75f - w * 0.5f; demo_slot_ = 3; }
  // Re-anchor y in case the viewport was resized.
  demo_->T().y = y;

  if (IsKeyPressed(KEY_J)) demo_->JuiceUp(0.4f, 0.0f);

  demo_->Move(dt);
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
  // POINT filter avoids the BILINEAR half-pixel smear that turns 1px grid
  // lines into uneven 1/2px streaks when ImGui::Image scales the RT by a
  // non-integer factor. Text stays crisp because engine::text bakes its
  // atlas at 2× super-sample, so atlas-level AA survives the nearest tap.
  SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
  target_w_ = w;
  target_h_ = h;
  target_valid_ = true;
}

void GameLayer::DrawScene() {
  BeginTextureMode(target_);
  ClearBackground(background_color_);

  // Reference grid so resizes are visible. Translucent white reads cleanly
  // against any theme background instead of fighting a fixed dark color.
  const Color kGridColor{255, 255, 255, 32};
  for (int x = 0; x < target_w_; x += 32) {
    DrawLine(x, 0, x, target_h_, kGridColor);
  }
  for (int y = 0; y < target_h_; y += 32) {
    DrawLine(0, y, target_w_, y, kGridColor);
  }

  // The Sprite demo: position+size from VT (eased), centered rotation,
  // atlas slice picked at OnAttach.
  if (demo_) demo_->Render();

  engine::DrawTextBold(TextFormat("Slot %d  (1/2/3 to move, J to juice)", demo_slot_), Vector2{16, 16}, 18, RAYWHITE);
  engine::DrawText(
      TextFormat("T.x=%.1f  VT.x=%.1f  juice=%s",
                 demo_ ? demo_->T().x : 0.0f,
                 demo_ ? demo_->VT().x : 0.0f,
                 (demo_ && demo_->HasJuice()) ? "yes" : "no"),
      Vector2{16, 44}, 18, Color{180, 180, 200, 255});

  EndTextureMode();
}

void GameLayer::DrawViewportPanel() {
  if (!show_viewport_) return;

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoScrollWithMouse;
  if (viewport_no_titlebar_) flags |= ImGuiWindowFlags_NoTitleBar;

  // Zero padding so the framebuffer fills the entire panel.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
  bool open = ImGui::Begin("Viewport", nullptr, flags);
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
  if (!show_hierarchy_) return;
  if (!ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse)) {
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
  if (!show_console_) return;
  if (!ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }
  ImGui::TextDisabled("[INFO] Game viewport initialized.");
  ImGui::TextDisabled("[INFO] Default dock layout applied.");
  ImGui::TextDisabled("[HINT] Right-click the viewport to toggle its title bar.");
  ImGui::End();
}
