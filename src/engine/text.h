#pragma once

#include <raylib.h>

#include <string_view>

namespace engine {

// Codepoint coverage for font atlas builds.
//
// raylib's LoadFontEx rasterises an atlas eagerly. Wider codepoint sets
// = bigger atlas + slower load, so MVP defaults to ASCII only and opts
// into CJK explicitly when the game starts pulling localized strings.
enum class CodepointSet {
  AsciiOnly,      // U+0020 .. U+007E
  AsciiPlusCJK,   // ASCII + U+3000..303F (CJK punct)
                  //       + U+4E00..9FFF (Unified Ideographs)
                  //       + U+FF00..FFEF (Fullwidth/Halfwidth)
};

// Initialise the global font cache. Idempotent — second call is a no-op.
// Pre-loads OpenSans Regular + Bold at a few common sizes so the first
// frame doesn't pause to rasterise. Lazy load handles anything else.
//
// `ui_scale` is the DPI-driven multiplier applied when rasterising the
// atlas: every requested logical size N is baked at `round(N * ui_scale)`
// physical pixels. Pass <= 0 (the default) to auto-detect from raylib's
// `GetWindowScaleDPI()` — that matches what ImGui does, so raylib text
// at logical size 18 renders the same visual height as ImGui at size 18.
//
// Must be called AFTER InitWindow (raylib needs a GL context to upload
// the font atlas texture).
void LoadFonts(CodepointSet cps = CodepointSet::AsciiOnly,
               float ui_scale = -1.0f);

// Free every cached Font. Must be called BEFORE CloseWindow (raylib
// needs the GL context to free the GPU textures).
void UnloadFonts();

// Look up (or lazy-load) the cached font for (name, logical_size).
// `logical_size` is UI points — the actual atlas was rasterised at
// `logical_size * ui_scale` physical pixels under the hood.
const Font& GetFont(std::string_view name, int logical_size);

// All Draw* and MeasureText below take *logical* sizes (UI points).
// Wrappers around DrawTextEx / MeasureTextEx that auto-pick the
// appropriately-rasterised atlas. Spacing fixed at 1 logical pt.
void DrawText(std::string_view text, Vector2 pos, int size, Color color);
void DrawTextBold(std::string_view text, Vector2 pos, int size, Color color);

Vector2 MeasureText(std::string_view text, int size);

}  // namespace engine
