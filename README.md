# card

Empty C++23 game scaffold built on top of raylib + Dear ImGui (docking) + raygui.
Stripped from the [`baba`](../baba) project — same layered architecture, no game
content yet.

## Tech stack

- **Language**: C++23, hardcoded to `-stdlib=libc++` (Clang only)
- **Build**: CMake ≥ 3.30 + Ninja (recommended)
- **Window / GL**: [raylib 6.0](https://github.com/raysan5/raylib)
- **Editor UI**: [Dear ImGui](https://github.com/ocornut/imgui) (docking branch),
  wired through GLFW + OpenGL3 backends
- **Header-only libs**: [raygui](https://github.com/raysan5/raygui),
  [entt](https://github.com/skypjack/entt),
  [nlohmann/json](https://github.com/nlohmann/json),
  [magic_enum](https://github.com/Neargye/magic_enum)

All third-party libraries are pinned as git submodules under `deps/`.

## Quick start (MSYS2 UCRT64 — default)

The project is developed on Windows under [MSYS2](https://www.msys2.org/) `UCRT64`.
Open the **MSYS2 UCRT64** shell and run:

```sh
# 1. Install toolchain (one-time)
pacman -S --needed \
  mingw-w64-ucrt-x86_64-clang \
  mingw-w64-ucrt-x86_64-libc++ \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  git

# 2. Clone with submodules
git clone --recurse-submodules <this-repo-url> card
cd card

# 3. Configure & build
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build

# 4. Run (from repo root — assets resolve relative to CWD)
./build/card.exe
```

If you forgot `--recurse-submodules` at clone time:

```sh
git submodule update --init --recursive
```

## Other Windows setups

### Non-MSYS2 (LLVM-only)

If you are not on MSYS2 but want the same toolchain on Windows:

1. Install [LLVM](https://github.com/llvm/llvm-project/releases) (Clang). The
   official Windows installer does **not** ship `libc++` — you have to point
   Clang at one yourself or build it.
2. Install [CMake](https://cmake.org/download/) and
   [Ninja](https://github.com/ninja-build/ninja/releases).
3. Same `cmake -S . -B build -G Ninja` invocation as above.

The cleanest path on Windows is still MSYS2 UCRT64; the LLVM-only flow only
makes sense if you can supply your own libc++.

### MSVC users

This project is **hardcoded to `-stdlib=libc++`** in `CMakeLists.txt`, and MSVC
does not ship libc++. You have two options:

1. **Recommended — use Clang.** Install LLVM/Clang or MSYS2 (see above) and
   build with `clang++` instead of `cl.exe`. The CMake config already skips the
   `-stdlib=libc++` flag when `MSVC` is the toolset, so a pure-MSVC build will
   compile against the MSVC STL — but this is **not tested** and you will likely
   hit warnings/errors from raylib's GLFW build.
2. **Build with `clang-cl` + libc++.** Get a Clang/LLVM build that bundles
   libc++ (e.g. via vcpkg or a self-built LLVM), then run `cmake -G Ninja`
   pointing `CMAKE_CXX_COMPILER` at that `clang-cl.exe`.

If you only have MSVC and want to drop the libc++ requirement entirely, edit
`CMakeLists.txt` and remove the `-stdlib=libc++` line. The code itself is plain
C++23 and does not depend on libc++ extensions.

## Linux / macOS

```sh
# Linux (Debian/Ubuntu)
sudo apt install clang libc++-dev libc++abi-dev cmake ninja-build

# macOS (Apple Clang already includes libc++)
brew install cmake ninja

git clone --recurse-submodules <this-repo-url> card
cd card
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/card
```

## Project layout

```
card/
├── assets/              # Runtime assets (loaded relative to CWD)
│   ├── icons/           # favicon.png / favicon.svg
│   └── fonts/opensans/  # ImGui UI font
├── cmake/               # CMake helpers (e.g. EnableCxxImportStd)
├── deps/                # Git submodules — third-party libs
│   ├── raylib/          # raylib 6.0
│   ├── imgui/           # Dear ImGui, docking branch
│   ├── raygui/          # immediate-mode GUI for raylib
│   ├── entt/            # ECS, header-only
│   ├── json/            # nlohmann/json, header-only
│   └── magic-enum/      # enum reflection, header-only
├── src/
│   ├── main.cpp         # Entry — instantiates Game and calls Run()
│   ├── game.{h,cpp}     # Lifecycle: Init → Tick(Update+Render) → Shutdown
│   ├── layer.h          # Base Layer interface
│   ├── layer_stack.{h,cpp}  # Ordered layers + overlays (overlays are last)
│   └── imgui_layer.{h,cpp}  # ImGui overlay (themes, inspector, demo)
├── CMakeLists.txt
├── run.sh               # Convenience: rebuild + run
└── README.md
```

## Architecture

`Game::Run()` is the canonical lifecycle:

```
Init()                  -> create window, init audio, push layers
loop:
  Tick()                -> Update() then Render()
    Update()            -> dispatch dt to every Layer::OnUpdate
    Render()            -> raylib draws, then ImGui overlay draws
Shutdown()              -> save window state, detach layers, close window
```

`LayerStack` keeps two ordered groups in a single vector:

- **Layers** — game systems, iterated first.
- **Overlays** — UI / debug, iterated last so they paint on top.

`ImGuiLayer` is pushed as an overlay. To add a game system, derive from `Layer`,
override `OnUpdate` / `OnRender` / `OnImGuiRender`, and `push_layer` it during
`Game::Init()`.

## Runtime keys

- `` ` `` (backtick) — toggle ImGui panels.
- `F11` — toggle borderless fullscreen.

## Conventions

- **UTF-8 + LF** for every text file (no BOM, no CRLF).
- **clang-format** (Google base, 2-space, 120 col, `PointerAlignment: Left`)
  is the formatting source of truth — see `.clang-format`.
- Run the binary from the **repo root**, not from `build/` — assets are
  resolved relative to CWD.
