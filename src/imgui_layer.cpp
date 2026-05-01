#include "imgui_layer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <raylib.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}

void ImGuiLayer::BindGamePanelToggles(bool* viewport, bool* hierarchy, bool* console, bool* viewport_no_titlebar) {
  show_viewport_ = viewport;
  show_hierarchy_ = hierarchy;
  show_console_ = console;
  viewport_no_titlebar_ = viewport_no_titlebar;
}

void ImGuiLayer::OnAttach() {
  const float dpi_scale = GetDpiScale();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  LoadFonts(dpi_scale);
  SetupStyle(dpi_scale);
  LoadThemes();

  GLFWwindow* window = glfwGetCurrentContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  ApplyTheme(selected_theme_);

  // If the user has no saved imgui.ini we apply our default game-engine
  // layout on the first frame. Subsequent runs reuse the user's saved layout.
  const char* ini = io.IniFilename ? io.IniFilename : "imgui.ini";
  needs_default_layout_ = !std::filesystem::exists(ini);
}

void ImGuiLayer::OnDetach() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void ImGuiLayer::OnUpdate(float /*dt*/) {
  if (IsKeyPressed(KEY_GRAVE)) {
    ToggleVisible();
  }
}

void ImGuiLayer::Begin() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiLayer::End() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow* backup = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup);
  }
}

void ImGuiLayer::OnImGuiRender() {
  // Visibility is gated by Game::Render before this runs, so no check here.

  DrawDockSpace();
  DrawMainMenuBar();

  if (show_inspector_) DrawInspectorPanel();
  if (show_themes_) DrawThemesPanel();

  if (show_demo_) {
    ImGui::ShowDemoWindow(&show_demo_);
  }
}

void ImGuiLayer::DrawDockSpace() {
  // PassthruCentralNode lets the raylib clear color show through where no
  // window is docked. We deliberately omit NoDockingOverCentralNode so the
  // Viewport panel can occupy the central node.
  const ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(
      0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

  if (needs_default_layout_) {
    needs_default_layout_ = false;
    SetupDefaultLayout(dockspace_id);
  }
}

void ImGuiLayer::SetupDefaultLayout(unsigned int dockspace_id) {
  const ImGuiID id = static_cast<ImGuiID>(dockspace_id);
  ImGui::DockBuilderRemoveNode(id);
  ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->Size);

  ImGuiID dock_main = id;
  const ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.18f, nullptr, &dock_main);
  const ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, nullptr, &dock_main);
  const ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.28f, nullptr, &dock_main);

  ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
  ImGui::DockBuilderDockWindow("Inspector", dock_right);
  ImGui::DockBuilderDockWindow("Themes", dock_right);
  ImGui::DockBuilderDockWindow("Console", dock_bottom);
  ImGui::DockBuilderDockWindow("Viewport", dock_main);

  ImGui::DockBuilderFinish(id);
}

void ImGuiLayer::DrawMainMenuBar() {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }

  if (ImGui::BeginMenu("File")) {
    ImGui::MenuItem("Quit", "Alt+F4", nullptr, false);
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("View")) {
    if (show_viewport_) ImGui::MenuItem("Viewport", nullptr, show_viewport_);
    if (show_hierarchy_) ImGui::MenuItem("Hierarchy", nullptr, show_hierarchy_);
    if (show_console_) ImGui::MenuItem("Console", nullptr, show_console_);
    ImGui::Separator();
    ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
    ImGui::MenuItem("Themes", nullptr, &show_themes_);
    ImGui::Separator();
    if (viewport_no_titlebar_) {
      ImGui::MenuItem("Hide Viewport Title Bar", nullptr, viewport_no_titlebar_);
    }
    if (ImGui::MenuItem("Reset Layout")) {
      needs_default_layout_ = true;
    }
    ImGui::Separator();
    ImGuiIO& io = ImGui::GetIO();
    bool viewports = (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
    if (ImGui::MenuItem("Allow Detach", nullptr, &viewports)) {
      if (viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
      } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
      }
    }
    ImGui::Separator();
    ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_);
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Help")) {
    ImGui::TextDisabled("Toggle UI: `");
    ImGui::TextDisabled("Borderless: F11");
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();
}

void ImGuiLayer::DrawInspectorPanel() {
  if (!show_inspector_) return;
  if (!ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }
  ImGui::TextDisabled("Empty scaffold. Add panels here.");
  ImGui::End();
}

void ImGuiLayer::DrawThemesPanel() {
  if (!show_themes_) return;
  if (!ImGui::Begin("Themes", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  if (ImGui::BeginCombo("Theme", themes_[selected_theme_].name.c_str())) {
    for (int i = 0; i < static_cast<int>(themes_.size()); ++i) {
      const bool is_selected = selected_theme_ == i;
      if (ImGui::Selectable(themes_[i].name.c_str(), is_selected)) {
        ApplyTheme(i);
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  if (ImGui::Button("Reset Theme")) {
    ApplyTheme(selected_theme_);
  }

  ImGui::Separator();
  ImGui::ColorEdit4("Background Color", background_color_.data());
  ImGui::Separator();
  ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

  ImGui::End();
}

void ImGuiLayer::ApplyTheme(int index) {
  selected_theme_ = std::clamp(index, 0, static_cast<int>(themes_.size()) - 1);
  background_color_ = themes_[selected_theme_].background;
}

std::vector<ImGuiLayer::Theme> ImGuiLayer::DefaultThemes() {
  return {
      {"Default", {0.14f, 0.14f, 0.14f, 1.0f}},
      {"Classic", {0.91f, 0.90f, 0.86f, 1.0f}},
  };
}

void ImGuiLayer::LoadThemes() {
  themes_ = DefaultThemes();
  const std::filesystem::path config_path = std::filesystem::path{"assets/themes.json"};
  if (!std::filesystem::exists(config_path)) {
    return;
  }

  try {
    std::ifstream in(config_path);
    if (!in.is_open()) {
      return;
    }

    nlohmann::json root;
    in >> root;

    const auto it = root.find("themes");
    if (it == root.end() || !it->is_array()) {
      return;
    }

    std::vector<Theme> loaded;
    for (const auto& item : *it) {
      if (!item.is_object()) {
        continue;
      }
      const auto name_it = item.find("name");
      const auto bg_it = item.find("background");
      if (name_it == item.end() || bg_it == item.end() || !name_it->is_string() || !bg_it->is_array() ||
          bg_it->size() != 4) {
        continue;
      }

      bool valid = true;
      ColorValue bg{};
      for (int i = 0; i < 4; ++i) {
        if (!(*bg_it)[i].is_number()) {
          valid = false;
          break;
        }
      }
      if (!valid) {
        continue;
      }

      bg.r = std::clamp((*bg_it)[0].get<float>(), 0.0f, 1.0f);
      bg.g = std::clamp((*bg_it)[1].get<float>(), 0.0f, 1.0f);
      bg.b = std::clamp((*bg_it)[2].get<float>(), 0.0f, 1.0f);
      bg.a = std::clamp((*bg_it)[3].get<float>(), 0.0f, 1.0f);

      loaded.push_back(Theme{name_it->get<std::string>(), bg});
    }

    if (!loaded.empty()) {
      themes_ = std::move(loaded);
    }
  } catch (...) {
    themes_ = DefaultThemes();
  }

  selected_theme_ = std::clamp(selected_theme_, 0, static_cast<int>(themes_.size()) - 1);
  background_color_ = themes_[selected_theme_].background;
}

void ImGuiLayer::LoadFonts(float dpi_scale) {
  ImGuiIO& io = ImGui::GetIO();

  const float font_size = kImGuiBaseFontSize * dpi_scale;
  const std::filesystem::path regular_font = std::filesystem::path{"assets/fonts/opensans/OpenSans-Regular.ttf"};
  const std::filesystem::path bold_font = std::filesystem::path{"assets/fonts/opensans/OpenSans-Bold.ttf"};

  io.Fonts->Clear();

  if (std::filesystem::exists(bold_font)) {
    io.Fonts->AddFontFromFileTTF(bold_font.string().c_str(), font_size);
  }

  if (std::filesystem::exists(regular_font)) {
    io.FontDefault = io.Fonts->AddFontFromFileTTF(regular_font.string().c_str(), font_size);
  }

  if (io.FontDefault == nullptr) {
    io.FontDefault = io.Fonts->AddFontDefault();
  }
}

void ImGuiLayer::SetupStyle(float dpi_scale) {
  ImGui::StyleColorsDark();

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 8.0f;
  style.GrabRounding = 8.0f;
  style.TabRounding = 8.0f;
  style.ScrollbarRounding = 8.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.WindowMenuButtonPosition = ImGuiDir_None;
  style.ScaleAllSizes(dpi_scale);

  auto& colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4{0.10f, 0.105f, 0.11f, 1.0f};
  colors[ImGuiCol_Header] = ImVec4{0.20f, 0.205f, 0.21f, 1.0f};
  colors[ImGuiCol_HeaderHovered] = ImVec4{0.30f, 0.305f, 0.31f, 1.0f};
  colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_Button] = ImVec4{0.20f, 0.205f, 0.21f, 1.0f};
  colors[ImGuiCol_ButtonHovered] = ImVec4{0.30f, 0.305f, 0.31f, 1.0f};
  colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_FrameBg] = ImVec4{0.20f, 0.205f, 0.21f, 1.0f};
  colors[ImGuiCol_FrameBgHovered] = ImVec4{0.30f, 0.305f, 0.31f, 1.0f};
  colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
  colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
  colors[ImGuiCol_TabUnfocused] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.20f, 0.205f, 0.21f, 1.0f};
  colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_DockingEmptyBg] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
}

Color ImGuiLayer::ToRaylibColor(const ColorValue& color) {
  auto to_byte = [](float value) {
    return (unsigned char)std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f);
  };

  return {
      to_byte(color.r),
      to_byte(color.g),
      to_byte(color.b),
      to_byte(color.a),
  };
}

float ImGuiLayer::GetDpiScale() {
  Vector2 dpi = GetWindowScaleDPI();
  return std::max(1.0f, std::max(dpi.x, dpi.y));
}
