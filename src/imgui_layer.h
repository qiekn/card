#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "layer.h"

class ImGuiLayer : public Layer {
 public:
  ImGuiLayer();

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float dt) override;
  void OnImGuiRender() override;

  // Begin/End bracket the per-frame ImGui pass around every layer's
  // OnImGuiRender. Keeping this explicit (rather than rolling it into
  // OnImGuiRender) lets the owner decide the exact ordering relative to
  // raylib draws and multi-viewport rendering.
  void Begin();
  void End();

  void ToggleVisible() { visible_ = !visible_; }
  bool IsVisible() const { return visible_; }

  Color BackgroundColor() const { return ToRaylibColor(background_color_); }

 private:
  struct ColorValue {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    float* data() { return &r; }
    const float* data() const { return &r; }
  };

  struct Theme {
    std::string name;
    ColorValue background;
  };

  void DrawMainMenuBar();
  void DrawInspectorPanel();
  void DrawThemesPanel();

  // Submits DockSpaceOverViewport and (on first run, when no imgui.ini exists)
  // installs the default game-engine dock layout via the DockBuilder API.
  void DrawDockSpace();
  void SetupDefaultLayout(unsigned int dockspace_id);

  void ApplyTheme(int index);
  void LoadThemes();
  void LoadFonts(float dpi_scale);
  void SetupStyle(float dpi_scale);

  static Color ToRaylibColor(const ColorValue& color);
  static float GetDpiScale();
  static std::vector<Theme> DefaultThemes();

  static constexpr float kImGuiBaseFontSize = 18.0f;

  bool visible_ = true;
  bool show_inspector_ = true;
  bool show_themes_ = true;
  bool show_demo_ = false;

  bool needs_default_layout_ = false;

  int selected_theme_ = 0;
  std::vector<Theme> themes_;
  ColorValue background_color_{};
};
