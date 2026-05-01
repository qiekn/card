---
source: src/{main,game,layer,layer_stack,game_layer,imgui_layer}.{h,cpp}
---

# Code Layout

`src/` 当前骨架（Phase 2 末）。从 [`baba`](../balatro/) 项目继承的"分层应用"
结构，**没有任何 Balatro 业务代码**——是一块空白画布。

## 1 · 文件清单

| 文件 | 角色 |
|--|--|
| `main.cpp` | 入口，`Game game; game.Run();` 三行 |
| `game.{h,cpp}` | 应用生命周期：Init / Tick / Shutdown |
| `layer.h` | `Layer` 抽象基类 |
| `layer_stack.{h,cpp}` | `LayerStack` 容器：layers + overlays |
| `imgui_layer.{h,cpp}` | ImGui backend + themes + 默认 docking 布局 |
| `game_layer.{h,cpp}` | 游戏主层：拥有离屏 RenderTexture，画到 Viewport 面板 |

## 2 · `Game` 生命周期

`game.h:8-35` 的 struct（POD-ish，没继承）：

```cpp
struct Game {
  void Run();   // Init → loop(Tick) → Shutdown
 private:
  void Init();    // 创窗 + 装 layer
  void Tick();    // = Update + Render
};
```

关键决策：

- **Game 自己拿 raylib window handle**（不抽掉）。Phase 3 加 `transform.h` 时，
  raylib 的全局 `GetScreenWidth()` / `GetFrameTime()` 直接进 layer，不引入
  额外的 wrapper。这是 raylib 风格，违反 Balatro 的 OO 模板但更轻。
- **borderless fullscreen** 独立成 `ToggleBorderless()`（`game.cpp:103-125`）：
  保存 windowed pos/size、切窗口 flag、设 monitor 尺寸 +1 px（避开 Windows
  的 exclusive fullscreen 优化路径）。这一坨细节不放进 layer。
- **窗口位置持久化**：`window.state` 文件 4 个 int（x y w h），`Init` 读、
  `Shutdown` 写。这是给"重启游戏窗口位置不变"的 dev QoL。

## 3 · `Layer` 接口

`layer.h:6-21`：

```cpp
class Layer {
 public:
  virtual void OnAttach() {}
  virtual void OnDetach() {}
  virtual void OnUpdate(float dt) {}
  virtual void OnRender() {}
  virtual void OnImGuiRender() {}
};
```

5 个虚函数全部默认空实现——子类按需覆盖。**不强制**让每个 layer 覆盖每个
hook。event 系统目前没有（注释里说"如果以后加，按 reverse 顺序遍历让 overlay
先吃 event"）。

## 4 · `LayerStack` — layers vs overlays

`layer_stack.h:12-45` 的核心字段：

```cpp
std::vector<std::unique_ptr<Layer>> layers_;
std::size_t overlay_start_ = 0;   // [0, overlay_start_) = layers
                                  // [overlay_start_, end) = overlays
```

单 vector 装两段，靠 `overlay_start_` 索引界。`push_layer` 插在前段尾，
`push_overlay` 推到 vector 末尾。**iterate 顺序就是 update / render 顺序**：
layers 先于 overlays。

ImGui 是 overlay（`game.cpp:84-85` 反过来——业务原因看下面）：

```cpp
// 注意：ImGuiLayer 先 push，但它是 overlay：
//   layers_.push_layer(std::move(imgui_layer));
//   layers_.push_layer(std::move(game_layer));
```

实际上 `ImGuiLayer` **不是用 `push_overlay` 加的**——是 `push_layer`，
但放在 `GameLayer` 之前。因为 ImGuiLayer 必须先 submit `DockSpaceOverViewport`
才能让 GameLayer 的 Viewport 窗口在第一帧就 dock 进来。这是 ImGui DockBuilder
API 的硬约束。

> **Caveat**：`LayerStack` 的 `push_overlay` 目前没人用——所有 layer 都用
> `push_layer`。等真有需要"绝对画在最上面"的 layer（debug overlay？）再启用。

## 5 · `Render` pipeline

`game.cpp:127-145` 的顺序很关键：

```cpp
BeginDrawing();
ClearBackground(...);
for (layer : layers_) layer->OnRender();  // raylib 画 layer
rlDrawRenderBatchActive();                // flush raylib batch

imgui_layer_->Begin();
for (layer : layers_) layer->OnImGuiRender();
imgui_layer_->End();

EndDrawing();
```

**rlDrawRenderBatchActive 的位置**是 baba 项目踩过的坑：raylib 用 batch
延迟 GL draw call，ImGui 一开就抢走 GL state，batch 没 flush 的话 raylib
图元会丢。这一行**别动**。

## 6 · `GameLayer` 干什么

`game_layer.h:10-45` 拥有：
- `RenderTexture2D target_`：离屏 framebuffer，DrawScene 全画进去
- `target_w_/h_/valid_`：跟 ImGui Viewport 面板尺寸对齐
- 4 个 `bool show_*`：面板可见性（菜单栏勾选）

绘制流：`OnRender` → 检查 viewport 尺寸是否变 → 重建 RenderTexture →
`BeginTextureMode` → DrawScene → `EndTextureMode`。
`OnImGuiRender` → 把 `target_.texture` 当 ImGui Image 贴在 Viewport 窗口。

> **Caveat**：`time_ = 0.0f`（`game_layer.h:44`）是 demo 用，渲染一个
> 旋转方块。Phase 3 起换成真业务 state（先 Movable，再 Card）。

## 7 · 接口边界（什么会改、什么不会）

**不会改**（Phase 3+ 也保留）：
- `Game::Run / Init / Tick / Shutdown` 四件套——应用生命周期
- `Layer` 5 个虚函数签名
- `LayerStack` 的 layers/overlays 双区设计

**会改**：
- `GameLayer::DrawScene()` 内容（demo → 真 ECS / Card 渲染）
- `Game::Init` 加 `AssertAssetsPresent()`（asset-pipeline.md §5）
- `Game` 加 `Transform` / `Camera` 字段（Phase 3+）

新 system（Movable、Sprite、CardArea）会以**纯数据 + 自由函数**形式加入，
不是 Layer 的子类——Layer 只用来分应用层（业务 / UI / debug），不是
ECS 的 system 抽象。详见 `architecture/ecs-vs-oo.md`。
