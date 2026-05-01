---
source: CMakeLists.txt + .clang-format + .gitmodules
---

# Build System

CMake ≥ 3.30 + Ninja + Clang (libc++)。**所有约束都是硬约束**——开发者
踩到的任何"诶为什么不让我用 MSVC / GCC / system stdlib"都在这页解释。

## 1 · 全表

| 维度 | 选择 | 备选 | 拒绝原因 |
|--|--|--|--|
| Generator | Ninja | Make / VS | 多平台一致 + 增量快 |
| Compiler | Clang | GCC / MSVC | libc++ 模块化 + 跨平台 |
| stdlib | libc++ | libstdc++ / MSVC STL | C++23 features + 跨平台行为一致 |
| C++ 标准 | 23 | 20 | range / format / ranges::to |
| Build Type | Release（默认） | Debug | raylib 默认 fail-fast warning |
| Deps 管理 | git submodules | vcpkg / Conan / CMake FetchContent | 可审计、不打 CI 就能 build |

## 2 · `CMakeLists.txt` 关键段

`CMakeLists.txt:13-21`：

```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)         # 禁 GNU 扩展，强制纯 C++23
if (NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()
```

`-stdlib=libc++` 只对非 MSVC 加。MSVC 不带 libc++——README 里给了
"自己装 libc++ 或者删这一行用 MSVC STL（未测试）"两条出路。

`CMakeLists.txt:9-11` 默认 Release：

```cmake
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
```

raylib 没指定 build type 时会 print 警告，强制 Release 顺便压掉它。

## 3 · 子模块 sanity check

`CMakeLists.txt:27-47` 在 configure 阶段就 verify 6 个 critical 文件存在：
`raylib/CMakeLists.txt`、`imgui/imgui.h`、`entt/.../entt.hpp`、`json/...`、
`magic-enum/...`、`raygui/src/raygui.h`。任一缺失就 `FATAL_ERROR` 提示
`git submodule update --init --recursive`。

**为什么这么严**：CMake 默认会让 `add_subdirectory(deps/raylib)` 直接
fail with cryptic CMake error。提前 check + 给清晰指示比让用户读
CMake stderr 体验好得多。

## 4 · 依赖列表（git submodule）

```text
deps/
├── raylib/        v6.0       # 窗口 + GL + 音频
├── imgui/         docking    # Editor UI（dev only，build 时编译）
├── raygui/        latest     # immediate-mode UI（用于游戏内 HUD）
├── entt/          latest     # ECS（header only，Phase 3+ 用）
├── json/          latest     # nlohmann（atlases.json / themes.json）
└── magic-enum/    latest     # enum reflection（debug 输出）
```

**为什么不用 vcpkg / Conan**：
- 这些工具自身有 setup 成本（注册账号、装 manifest 工具）
- submodule 锁版本到 commit hash，可审计性最高
- 把所有依赖物理放在仓里，offline 也能 build

代价：clone 时要 `--recurse-submodules`，更新依赖要 `git submodule update`。

## 5 · `.clang-format`

Google base + 2 空格缩进 + 120 列 + `PointerAlignment: Left`。
**format 是 source of truth**——任何 `clang-format` 与代码不一致都按
clang-format 走，CI 不强制（暂无 lint job），靠 dev 自觉 + editor on-save。

> **为什么 120 列而不是 80**：raylib API 命名长（`SetWindowState(FLAG_...)`），
> 80 列会让一行 if 拆成 4 行。120 在屏幕宽高比 16:9 下并排 2 个 file
> 还是放得下。

## 6 · 平台路径

| 平台 | 推荐 shell | 备注 |
|--|--|--|
| Windows | MSYS2 UCRT64 | `pacman -S mingw-w64-ucrt-x86_64-clang libc++ cmake ninja` |
| Linux | bash | `apt install clang libc++-dev libc++abi-dev cmake ninja-build` |
| macOS | bash / zsh | Apple Clang 自带 libc++，`brew install cmake ninja` |

**为什么 UCRT64 而不是 MINGW64**：UCRT 是新版 Windows runtime，跟 native
Windows API 行为更一致；MINGW64 用旧 msvcrt，时区 / locale 行为有怪坑。
新机器装 MSYS2 默认就开 UCRT64。

## 7 · Caveats

- **CI 没有 build job**：当前 `.github/workflows/` 只有 mdbook 部署。
  Phase 3 真开始有 C++ 代码时建议加一个 `ubuntu-latest + clang + ninja`
  的 build matrix 跑 `cmake --build`。
- **没 lint job**：clang-format / clang-tidy 都没接 CI。dev 数 ≤ 2 时
  自觉够用；超 3 人时再加 pre-commit hook。
- **没 test 框架**：MVP 阶段不需要 unit test（用户原话："Game logic
  test = play the game"）。Phase 7+ 接 doctest 或 catch2 时再选。
- **没 install target**：`cmake --install` 不工作。MVP 直接跑 build/
  下的 binary，未来要做 release 包再加。
