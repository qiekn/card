#include "game_layer.h"

#include <algorithm>
#include <memory>

#include <imgui.h>

#include "engine/text.h"
#include "game/card.h"

namespace {
// Display size of one card. 71×95 is the 1x atlas baseline; ×4 lands on
// 284×380, matching the Phase 4 single-card demo and reading well even
// on a 4K viewport (cards visibly overlap to form a fan).
constexpr float kCardW = 71.0f * 4.0f;
constexpr float kCardH = 95.0f * 4.0f;

constexpr int kHandSoftCap = 10;        // user-side N-key cap
constexpr int kHandTempLimit = 8;       // CardArea slot reservation

// Hand area pixel rect, computed against the current RT size. Cards spawn
// at the right edge of this rect so new cards "deal" from the right rather
// than flying in from the viewport corner.
struct HandLayout { float x, y, w, h; };
constexpr float kHandMaxW = 1600.0f;  // cap on wide viewports to keep cards readable
HandLayout ComputeHandLayout(int target_w, int target_h) {
  // 0.95 keeps cards densely fanned on narrow viewports (slot ~40% of
  // card_w visible at 8 cards). On very wide viewports we cap at
  // kHandMaxW so the hand doesn't spread cards out enough to lose the
  // overlapping fan silhouette.
  const float w = std::min(static_cast<float>(target_w) * 0.95f, kHandMaxW);
  const float x = (static_cast<float>(target_w) - w) * 0.5f;
  const float y = static_cast<float>(target_h) - kCardH - 80.0f;
  return {x, y, w, kCardH};
}

std::unique_ptr<game::Card> MakeJokerCard(const engine::Atlas& atlas, int idx, float spawn_x, float spawn_y) {
  // Walk through the Joker atlas grid (10 cols × 5 rows in 1x baseline).
  const int col = idx % 10;
  const int row = (idx / 10) % 5;
  return std::make_unique<game::Card>(spawn_x, spawn_y, kCardW, kCardH, atlas, col, row);
}
}  // namespace

GameLayer::GameLayer() : Layer("GameLayer") {}

GameLayer::~GameLayer() = default;

void GameLayer::OnAttach() {
  // Allocate something non-zero so the first frame has a valid texture even
  // before the Viewport panel reports its real size.
  EnsureTarget(1280, 720);

  // Eager-load every atlas in the manifest at the user-configured tier.
  // Failure leaves the registry empty and atlases_.Tier() at 0; OnUpdate's
  // mismatch path will revert texture_scale_ on the next tier flip.
  atlases_.Load("assets/atlases.json", texture_scale_);

  // Build the hand area at the bottom of the (initial) viewport. SetBounds
  // refreshes this every frame in OnUpdate so a viewport resize keeps the
  // hand pinned to the bottom-center.
  const HandLayout init = ComputeHandLayout(target_w_, target_h_);
  hand_.emplace(init.x, init.y, init.w, init.h, game::CardAreaType::Hand, kCardW, kHandTempLimit);

  if (const engine::Atlas* joker = atlases_.Find("Joker")) {
    const float spawn_x = init.x + init.w;  // right edge of hand area
    const float spawn_y = init.y;
    for (int i = 0; i < 5; ++i) {
      hand_->Emplace(MakeJokerCard(*joker, next_sprite_idx_++, spawn_x, spawn_y));
    }
    // Snap initial cards into their slots — without this they'd all visibly
    // fly in from spawn_x/y on the very first frame, which feels wrong for
    // a "scene loaded with a hand already dealt" state.
    hand_->HardSetCards(0.0f);
  }
}

void GameLayer::OnDetach() {
  if (target_valid_) {
    UnloadRenderTexture(target_);
    target_valid_ = false;
  }
  hand_.reset();
}

void GameLayer::OnUpdate(float dt) {
  time_ += dt;

  // Themes panel may have flipped the tier — try to apply. On miss we
  // revert the UI value so the combo never lies about what's loaded.
  // Registry reload is in-place move-assign, so each Card's borrowed
  // Atlas pointer stays valid; the only side-effect is its src rect
  // gets bigger / smaller at draw time via CellPx/Py.
  if (texture_scale_ != atlases_.Tier()) {
    if (!atlases_.Load("assets/atlases.json", texture_scale_)) {
      texture_scale_ = atlases_.Tier();
    }
  }

  if (!hand_) return;

  // Re-anchor the hand to the bottom-center of the viewport so it follows
  // panel resizes.
  const HandLayout layout = ComputeHandLayout(target_w_, target_h_);
  hand_->SetBounds(layout.x, layout.y, layout.w, layout.h);

  // N adds a card (capped at kHandSoftCap), M pops the rightmost. New
  // cards spawn at the right edge of the hand area so they slide in from
  // the right rather than flying in from (0,0).
  if (IsKeyPressed(KEY_N) && hand_->Size() < kHandSoftCap) {
    if (const engine::Atlas* joker = atlases_.Find("Joker")) {
      const float spawn_x = layout.x + layout.w;
      const float spawn_y = layout.y;
      hand_->Emplace(MakeJokerCard(*joker, next_sprite_idx_++, spawn_x, spawn_y));
    }
  }
  if (IsKeyPressed(KEY_M) && hand_->Size() > 0) {
    hand_->RemoveBack();
  }

  hand_->Tick(dt, time_);
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

  // The Phase 5 hand demo: 5 jokers fanned at the bottom of the viewport.
  // CardArea::Tick already wrote each card's T this frame; Render walks
  // the slot list left-to-right (no z-order yet — Phase 6+ will need
  // proper hover-lift z handling).
  if (hand_) hand_->Render();

  engine::DrawTextBold(TextFormat("Hand: %zu/%d   (N add, M remove, click highlight, drag reorder)",
                                  hand_ ? hand_->Size() : 0u, kHandSoftCap),
                       Vector2{16, 16}, 18, RAYWHITE);
  engine::DrawText(TextFormat("atlas tier=%dx   real_time=%.1fs", texture_scale_, time_),
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

    // Card hover / click / drag arbitration. mouse_rt stays valid even when
    // the cursor leaves the image — useful while dragging off-edge.
    const ImVec2 image_min = ImGui::GetItemRectMin();
    const ImVec2 m = ImGui::GetMousePos();
    const Vector2 mouse_rt{m.x - image_min.x, m.y - image_min.y};
    const bool image_hovered = ImGui::IsItemHovered();

    if (hand_) {
      // Hand cursor on hover (only when not already dragging — dragging
      // gets the system "grabbing" cursor implicitly via ImGui's drag).
      if (image_hovered && !hand_->IsDragging() && hand_->FindHovered(mouse_rt)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      }

      // Mouse-down inside the image picks the candidate card. Outside
      // clicks (menu bar, panels) are ignored because IsItemHovered
      // gates this branch.
      if (image_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressed_card_ = hand_->FindHovered(mouse_rt);
        pressed_origin_rt_ = mouse_rt;
      }

      // While the mouse stays down on a candidate: promote to drag once
      // ImGui's drag threshold is crossed, then forward cursor updates.
      if (pressed_card_ != nullptr) {
        if (!hand_->IsDragging() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          hand_->StartDrag(pressed_card_, pressed_origin_rt_);
        }
        if (hand_->IsDragging()) {
          hand_->UpdateDrag(mouse_rt);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          if (hand_->IsDragging()) {
            hand_->StopDrag();
          } else {
            // Click without drag → toggle highlight + JuiceUp feedback.
            pressed_card_->SetHighlighted(!pressed_card_->Highlighted());
            pressed_card_->JuiceUp(0.4f, 0.0f);
          }
          pressed_card_ = nullptr;
        }
      }
    }
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
