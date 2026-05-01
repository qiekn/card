#pragma once

#include <string>
#include <unordered_map>

#include "engine/sprite.h"

namespace engine {

// AtlasRegistry — eager-loads every atlas declared in `assets/atlases.json`
// for a given texture-scale tier and hands them out by name. Mirrors what
// Balatro does ad-hoc in `Game:set_atli`: a single map keyed by short
// name (`"Joker"`, `"cards_1"`, ...) with the loaded Texture2D + per-cell
// metadata behind it.
//
// The manifest's `px` / `py` are the **1x baseline** (matches the lua
// `atlas` table). Load(tier) prefixes each entry's `path` with
// `assets/textures/{tier}x/` and multiplies cell dims by `tier`, so the
// same JSON drives every available resolution.
//
// Pointer-stability contract: re-calling Load() to switch tiers
// move-assigns into existing map entries instead of clearing, so any
// `const Atlas*` previously handed out by Find() stays valid. This is
// what lets Sprite / Card hold a non-owning pointer across the Themes
// panel's "Texture Scale" combo flips.
class AtlasRegistry {
 public:
  AtlasRegistry() = default;
  ~AtlasRegistry() = default;

  AtlasRegistry(const AtlasRegistry&) = delete;
  AtlasRegistry& operator=(const AtlasRegistry&) = delete;

  // Loads or reloads every atlas in `manifest_path` for `tier` (1 or 2 in
  // current MVP). Returns true if at least one atlas loaded — false
  // signals "manifest missing / target dir absent", so the caller can
  // revert the UI tier. Per-atlas misses only fprintf-warn (Atlas ctor),
  // matching the rest of the asset stack's soft-fail policy.
  bool Load(const char* manifest_path, int tier);

  // Returns nullptr if `name` is not in the manifest. The returned Atlas
  // may have Loaded() == false if its file was missing on disk.
  const Atlas* Find(const char* name) const;

  int Tier() const { return tier_; }

 private:
  std::unordered_map<std::string, Atlas> atlases_;
  int tier_ = 0;
};

}  // namespace engine
