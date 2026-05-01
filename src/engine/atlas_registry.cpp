#include "engine/atlas_registry.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace engine {

bool AtlasRegistry::Load(const char* manifest_path, int tier) {
  if (tier <= 0) return false;

  const std::filesystem::path manifest{manifest_path};
  if (!std::filesystem::exists(manifest)) {
    std::fprintf(stderr, "[atlas-registry] manifest not found: %s\n", manifest_path);
    return false;
  }

  nlohmann::json root;
  try {
    std::ifstream in(manifest);
    in >> root;
  } catch (...) {
    std::fprintf(stderr, "[atlas-registry] manifest parse error: %s\n", manifest_path);
    return false;
  }

  if (!root.is_array()) {
    std::fprintf(stderr, "[atlas-registry] manifest must be a JSON array: %s\n", manifest_path);
    return false;
  }

  int loaded_ok = 0;
  for (const auto& item : root) {
    if (!item.is_object()) continue;
    const auto name_it = item.find("name");
    const auto path_it = item.find("path");
    const auto px_it = item.find("px");
    const auto py_it = item.find("py");
    if (name_it == item.end() || !name_it->is_string()) continue;
    if (path_it == item.end() || !path_it->is_string()) continue;
    if (px_it == item.end() || !px_it->is_number_integer()) continue;
    if (py_it == item.end() || !py_it->is_number_integer()) continue;

    const std::string name = name_it->get<std::string>();
    const std::string path = path_it->get<std::string>();
    const int px = px_it->get<int>() * tier;
    const int py = py_it->get<int>() * tier;

    char full[256];
    std::snprintf(full, sizeof(full), "assets/textures/%dx/%s", tier, path.c_str());

    Atlas next{full, px, py};
    const bool ok = next.Loaded();

    // Move-assign into the existing slot so any const Atlas* we previously
    // handed out stays valid across reloads. New names emplace fresh.
    auto it = atlases_.find(name);
    if (it == atlases_.end()) {
      atlases_.emplace(name, std::move(next));
    } else {
      it->second = std::move(next);
    }

    if (ok) ++loaded_ok;
  }

  if (loaded_ok == 0) {
    std::fprintf(stderr,
                 "[atlas-registry] no atlases loaded for %dx — wrong tier or missing assets/textures/%dx/?\n",
                 tier, tier);
    return false;
  }

  tier_ = tier;
  return true;
}

const Atlas* AtlasRegistry::Find(const char* name) const {
  auto it = atlases_.find(name);
  return it == atlases_.end() ? nullptr : &it->second;
}

}  // namespace engine
