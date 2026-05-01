#include "engine/text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
namespace {

constexpr const char* kRegularPath = "assets/fonts/opensans/OpenSans-Regular.ttf";
constexpr const char* kBoldPath = "assets/fonts/opensans/OpenSans-Bold.ttf";

// Pre-loaded sizes (in logical UI pt) — covers HUD common range.
// At ui_scale=2.0 these become 32/36/40/48/64 physical px atlases.
constexpr int kPreloadSizes[] = {16, 18, 20, 24, 32};

// Super-sampling factor for the atlas. We bake the bitmap at 2× the
// rendered size so DrawTextEx scales the atlas DOWN by 0.5× at draw time.
// Why: raylib's stb_truetype path has visible baseline rounding artifacts
// (descenders of 'j' / 'g' clip; '1' sits a sub-pixel above the line) when
// baked at small target sizes. A larger bake + downscale dodges both.
constexpr int kAtlasSuperSample = 2;

constexpr float kDefaultSpacing = 1.0f;

struct FontKey {
  std::string name;
  int logical_size = 0;
  bool operator==(const FontKey& o) const noexcept {
    return logical_size == o.logical_size && name == o.name;
  }
};

struct FontKeyHash {
  std::size_t operator()(const FontKey& k) const noexcept {
    return std::hash<std::string>{}(k.name) ^
           (static_cast<std::size_t>(k.logical_size) * 0x9E3779B97F4A7C15ULL);
  }
};

// Per-font cached entry: the rasterised atlas + the physical size it
// was baked at (so DrawTextEx can pass the matching fontSize for crisp
// 1:1 sampling).
struct FontEntry {
  Font font{};
  int physical_size = 0;
};

std::unordered_map<FontKey, FontEntry, FontKeyHash> g_cache;
std::vector<int> g_codepoints;
float g_ui_scale = 1.0f;

void BuildCodepoints(CodepointSet cps) {
  g_codepoints.clear();
  // ASCII printable range. (Newline / tab handled by DrawTextEx itself.)
  for (int c = 0x20; c <= 0x7E; ++c) g_codepoints.push_back(c);

  if (cps == CodepointSet::AsciiPlusCJK) {
    // CJK Symbols and Punctuation — 、 。 「 」 etc.
    for (int c = 0x3000; c <= 0x303F; ++c) g_codepoints.push_back(c);
    // CJK Unified Ideographs — bulk of common Chinese / Japanese kanji.
    for (int c = 0x4E00; c <= 0x9FFF; ++c) g_codepoints.push_back(c);
    // Halfwidth / Fullwidth Forms.
    for (int c = 0xFF00; c <= 0xFFEF; ++c) g_codepoints.push_back(c);
  }
}

const char* PathFor(std::string_view name) {
  if (name == "bold") return kBoldPath;
  return kRegularPath;
}

int Physical(int logical_size) {
  return static_cast<int>(std::round(logical_size * g_ui_scale));
}

float DetectUiScale() {
  // Mirrors ImGuiLayer::GetDpiScale — read raylib's per-axis DPI and
  // take the max, never below 1.0. Keeps raylib text the same visual
  // size as ImGui text on HiDPI displays.
  Vector2 dpi = GetWindowScaleDPI();
  return std::max(1.0f, std::max(dpi.x, dpi.y));
}

FontEntry LoadOne(const char* path, int logical_size) {
  FontEntry entry{};
  entry.physical_size = Physical(logical_size);
  const int bake_size = entry.physical_size * kAtlasSuperSample;
  if (!std::filesystem::exists(path)) {
    std::fprintf(stderr, "engine::Text: font file missing: %s — falling back\n", path);
    entry.font = GetFontDefault();
    return entry;
  }
  entry.font =
      LoadFontEx(path, bake_size, g_codepoints.data(), static_cast<int>(g_codepoints.size()));
  // The atlas is now higher-res than we render at — bilinear downsample
  // gives clean glyphs. POINT here would alias hard.
  SetTextureFilter(entry.font.texture, TEXTURE_FILTER_BILINEAR);
  return entry;
}

const FontEntry& GetOrLoad(std::string_view name, int logical_size) {
  FontKey key{std::string(name), logical_size};
  if (auto it = g_cache.find(key); it != g_cache.end()) {
    return it->second;
  }
  FontEntry entry = LoadOne(PathFor(name), logical_size);
  auto [it, _] = g_cache.emplace(std::move(key), entry);
  return it->second;
}

}  // namespace

void LoadFonts(CodepointSet cps, float ui_scale) {
  if (!g_cache.empty()) return;  // idempotent
  g_ui_scale = (ui_scale > 0.0f) ? ui_scale : DetectUiScale();
  BuildCodepoints(cps);
  for (int s : kPreloadSizes) {
    g_cache[{"regular", s}] = LoadOne(kRegularPath, s);
    g_cache[{"bold", s}] = LoadOne(kBoldPath, s);
  }
}

void UnloadFonts() {
  for (auto& [_, entry] : g_cache) {
    // Don't unload raylib's default font — it owns its own lifecycle.
    if (entry.font.texture.id != GetFontDefault().texture.id) {
      UnloadFont(entry.font);
    }
  }
  g_cache.clear();
  g_codepoints.clear();
  g_ui_scale = 1.0f;
}

const Font& GetFont(std::string_view name, int logical_size) {
  return GetOrLoad(name, logical_size).font;
}

void DrawText(std::string_view text, Vector2 pos, int size, Color color) {
  std::string s(text);
  const FontEntry& e = GetOrLoad("regular", size);
  // fontSize = physical so DrawTextEx hits the atlas 1:1 (crisp); pos is
  // logical (caller's RT space, where ImGui::Image scales RT to display).
  // Spacing scales with the same factor so kerning stays proportional.
  DrawTextEx(e.font, s.c_str(), pos, static_cast<float>(e.physical_size),
             kDefaultSpacing * g_ui_scale, color);
}

void DrawTextBold(std::string_view text, Vector2 pos, int size, Color color) {
  std::string s(text);
  const FontEntry& e = GetOrLoad("bold", size);
  DrawTextEx(e.font, s.c_str(), pos, static_cast<float>(e.physical_size),
             kDefaultSpacing * g_ui_scale, color);
}

Vector2 MeasureText(std::string_view text, int size) {
  std::string s(text);
  const FontEntry& e = GetOrLoad("regular", size);
  return MeasureTextEx(e.font, s.c_str(), static_cast<float>(e.physical_size),
                       kDefaultSpacing * g_ui_scale);
}

}  // namespace engine
