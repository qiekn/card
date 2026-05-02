---
source: src/engine/sprite.{h,cpp}, ref-balatro/engine/sprite.lua:1-200
---

# Sprite

Phase 4 的内容。Movable 已经能算"现在到哪了"，但当前 demo 画的是
`DrawRectanglePro` 红方块——下一步把方块换成真正的卡。

## 1 · 问题：50 张牌得画 50 张图

最朴素的写法：每张卡一份 PNG，`LoadTexture` 出来一份 `Texture2D`，
画的时候 `DrawTexture(card.tex, x, y, WHITE)`。

跑得起来，但 GPU 每帧要切 50 次纹理 binding。绑定是 OpenGL state
change，不便宜——一桌手牌就把 batching 掉光。Balatro 的解法是 atlas
（贴图集）：**一张大 PNG 塞所有同尺寸的图**，画的时候只切矩形坐标。

`assets/balatro/textures/1x/Jokers.png` 是这样：

```
┌──┬──┬──┬──┬──┬──┬──┬──┐
│J0│J1│J2│J3│J4│J5│J6│J7│   每格 71×95
├──┼──┼──┼──┼──┼──┼──┼──┤
│J8│J9│ ...
```

要画第 5 个 Joker（`sprite_pos = {5, 0}`），src 矩形 = `(5*71, 0*95,
71, 95)`。50 张 Joker 一次绑定、50 次 draw call、纹理切换 0 次。

## 2 · Atlas：RAII 包一张大图

`Atlas` 的责任：持 raylib `Texture2D` + 单格尺寸 `(px, py)`，析构自动
`UnloadTexture`，move 走所有权。

**Listing 1**: `src/engine/sprite.h`

```cpp
class Atlas {
 public:
  Atlas() = default;
  Atlas(const char* path, int px, int py);
  ~Atlas();

  Atlas(const Atlas&) = delete;
  Atlas& operator=(const Atlas&) = delete;
  Atlas(Atlas&& other) noexcept;
  Atlas& operator=(Atlas&& other) noexcept;

  bool Loaded() const { return texture_.id != 0; }
  const Texture2D& Texture() const { return texture_; }
  int CellPx() const { return px_; }
  int CellPy() const { return py_; }

 private:
  Texture2D texture_{};   // .id == 0 means empty / moved-from
  int px_ = 0;
  int py_ = 0;
};
```

raylib 的 `Texture2D` 是 POD（id + width + height + ...）。我们用
`id == 0` 当 sentinel "empty"——dtor 只在 `id != 0` 时
`UnloadTexture`，move ctor / op= 把源 id 清零防 double-free。比
`std::unique_ptr<Texture2D, Deleter>` 短一截，对 POD 这么用没有析构
异常之类的问题。

**Listing 2**: `src/engine/sprite.cpp` move 实现

```cpp
Atlas::Atlas(Atlas&& other) noexcept
    : texture_(other.texture_), px_(other.px_), py_(other.py_) {
  other.texture_ = {};
  other.px_ = 0;
  other.py_ = 0;
}

Atlas& Atlas::operator=(Atlas&& other) noexcept {
  if (this != &other) {
    if (texture_.id != 0) UnloadTexture(texture_);
    texture_ = other.texture_;
    px_ = other.px_;
    py_ = other.py_;
    other.texture_ = {};
  }
  return *this;
}
```

ctor 也做一件别的事：load 完后 `SetTextureFilter(TEXTURE_FILTER_POINT)`。
原因第 5 节讲。

## 3 · Sprite：继承 Movable，加 atlas 引用

lua 端 `Sprite : Moveable`。我们直译——`engine::Sprite` 继承
`engine::Movable`，复用 T/VT/juice 的所有机制；多两个字段：atlas
指针 + grid coord。

**Listing 3**: `src/engine/sprite.h`

```cpp
class Sprite : public Movable {
 public:
  Sprite(float x, float y, float w, float h, const Atlas& atlas,
         int sprite_pos_x, int sprite_pos_y);

  void Render() override;

  void SetSpritePos(int x, int y) { sprite_x_ = x; sprite_y_ = y; }

 private:
  const Atlas* atlas_ = nullptr;  // non-owning; outlive the Sprite.
  int sprite_x_ = 0;
  int sprite_y_ = 0;
};
```

Atlas 持 `const Atlas*` 而不是 `const Atlas&`：因为 Sprite 也想
move-only（继承 Movable，Movable 的 Velocity / Juice 不让拷），引用
不能 reseat 也不能默认构造。指针寿命由调用方保证——把 Atlas 放成员
列表前面，Sprite 后面，析构反序自动对。

## 4 · Render：lua 整条链压成一个 DrawTexturePro

lua 的 `Sprite:draw_self`（`sprite.lua:158-190`）配合 `prep_draw`
（`misc_functions.lua:968`）走的是 push/translate/rotate/translate/scale
的 OpenGL matrix 链。raylib 的 `DrawTexturePro` 自带 `origin` 参数，
把"绕中心旋转"内化掉——一行调用搞定。

**Listing 4**: `src/engine/sprite.cpp::Render`

```cpp
void Sprite::Render() {
  if (atlas_ == nullptr || !atlas_->Loaded()) return;

  const Transform& vt = VT();
  const float draw_w = vt.w * vt.scale;
  const float draw_h = vt.h * vt.scale;

  // src: 从 atlas 网格里挑出 (sprite_x, sprite_y) 那一格。
  const Rectangle src{
      static_cast<float>(sprite_x_ * atlas_->CellPx()),
      static_cast<float>(sprite_y_ * atlas_->CellPy()),
      static_cast<float>(atlas_->CellPx()),
      static_cast<float>(atlas_->CellPy()),
  };
  // dst: VT 驱动的目标矩形，旋转绕中心。
  const Rectangle dst{vt.x + vt.w * 0.5f, vt.y + vt.h * 0.5f,
                      draw_w, draw_h};
  const Vector2 origin{draw_w * 0.5f, draw_h * 0.5f};
  const float deg = vt.r * 57.2957795f;  // raylib wants degrees

  DrawTexturePro(atlas_->Texture(), src, dst, origin, deg, WHITE);
}
```

四件事：

- `src` 从 atlas 网格切——`sprite_pos.x * atlas.px` 是 lua
  `set_sprite_pos` 那个 `newQuad` 的直译。
- `dst` 中心点 = `VT.x + VT.w/2`（不是左上角），尺寸 = `VT.w *
  VT.scale`——这样 juice 的 scale wobble 自然把卡缩 / 弹。
- `origin` 是局部坐标里的旋转中心——给 `(draw_w/2, draw_h/2)` 就让
  raylib 把 dst 矩形围着自己中心转。
- 弧度 → 度的 `57.2957795` 是 `180/π`——`Transform.r` 用弧度（lua
  原汁原味），raylib API 要度，画的那一刻乘一下。

`Transform` 注释里说过"像素换算留到画的那一刻"——MVP 这一阶段
`T.{w,h}` 直接是像素，没引入 `G.TILESCALE * G.TILESIZE` 那一层。
等 CardArea 真要用 game-unit（譬如 `card.T.w = 2.5` 表示 2.5 个 tile）
再加。

## 5 · BILINEAR filter：从 POINT 翻过来

最初这一节写的是"POINT 写死"——理由是 Balatro 是 pixel art、bilinear
会把 1px 描边糊成 2px。Phase 5 接 hand 扇形 + drag 之后 4K 屏上发现
旋转卡的边缘锯齿很明显：每张 hand 卡都有 ±0.1 rad 倾角，POINT 采样
在卡边界两侧落整数 texel → 阶梯。

权衡：

- **POINT**：整数 scale 下纹理 crisp，但旋转 / 非整数 scale 下边缘
  阶梯化。1px 描边在 axis-aligned 时锐利，在 rotated 时锯齿。
- **BILINEAR**：4 texel 加权混合。旋转 / 非整数 scale 下边缘平滑过渡。
  非整数 scale 下纹理内容（数字 / 花色 pip）轻微软化。

我们的场景：每张 hand 卡都旋转。pixel art 内容显示在 4× 放大尺寸
（71→284 px），texture 软化在 4K 屏高 PPI 下肉眼几乎不见。结论：
BILINEAR 收益大于损失。

```cpp
SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
```

写死，没给构造参数让 caller 选——所有 hand 卡都是 pixel art 但都旋
转，统一 BILINEAR。要真有"完全 axis-aligned + 极小尺度需要 crisp
1px 线"的需求（譬如 ui_assets atlas 画细 UI 边框）再加 enum 参数。

注意叠加效应：viewport RT 仍是 POINT（`game_layer.cpp` 设过）——RT
1:1 显示到 ImGui::Image 时不需要 filter；如果出现非整数 scale，RT
也跟着改 BILINEAR。**整条链 atlas BILINEAR → RT POINT → ImGui scale
nearest**，旋转锯齿在 atlas → RT 阶段就消除了。

## 6 · 接进 demo：viewport 中央放一张 Joker

GameLayer 拿掉旧的红方块 demo，换成 Sprite。Atlas 是成员，OnAttach
load 一次，OnDetach reset；Sprite 是 `std::optional<engine::Sprite>`
（move-only + 要 Atlas，没法默认构造），OnAttach 里 emplace。

**Listing 5**: `src/game_layer.h` 关键字段

```cpp
engine::Atlas joker_atlas_;
std::optional<engine::Sprite> demo_;
```

**Listing 6**: `src/game_layer.cpp::OnAttach` 关键几行

```cpp
joker_atlas_ = engine::Atlas{
    "assets/balatro/textures/1x/Jokers.png", 71, 95};

constexpr float kCardScale = 4.0f;
const float w = 71.0f * kCardScale;
const float h = 95.0f * kCardScale;
const float center_x = static_cast<float>(target_w_) * 0.5f - w * 0.5f;
const float center_y = static_cast<float>(target_h_) * 0.5f - h * 0.5f;
demo_.emplace(center_x, center_y, w, h, joker_atlas_,
              /*sprite_x=*/0, /*sprite_y=*/0);
```

4× scale 让 71×95 的格子变成 284×380 px，在典型 viewport 尺寸下读得
出细节。`{0, 0}` 是 vanilla Joker（atlas 第一格）。

1/2/3 切位置、J 触发 juice 跟 Phase 3 一样——只是 `demo_.T()` 变成
`demo_->T()`：

```cpp
if (IsKeyPressed(KEY_ONE)) demo_->T().x = vw * 0.25f - w * 0.5f;
...
if (IsKeyPressed(KEY_J))   demo_->JuiceUp(0.4f, 0.0f);
demo_->Move(dt);
```

DrawScene 里红方块那段换成一行：

```cpp
if (demo_) demo_->Render();
```

跑起来：viewport 中央显示 Joker；按 1/3 切位置，VT 平滑追上去；
按 J 触发 squash & stretch——卡的 scale 弹一下，atlas 切片不变，
所以是同一张图在缩放，没切帧。

> **运行验证**：跑 `./build/card.exe` 进 Viewport 面板，应该看到
> Joker.png 第一格（vanilla Joker）。资产没装的话 stderr 一行
> `[atlas] failed to load ...`，viewport 只剩 grid 和 HUD——不崩。

## Caveats

- **没接 atlases.json**：硬编码 `71, 95` 在 OnAttach。Phase 5 加多
  atlas（CardArea 要 8BitDeck.png + Jokers.png 同时上）时再加 JSON
  registry——`std::unordered_map<std::string, Atlas>`，启动读
  `assets/atlases.json`。
- **资产缺失走 noop 不 exit(1)**：跟 architecture/asset-pipeline.md §5
  写的 `AssertAssetsPresent` 不一致。MVP 选 noop 因为没装 Balatro
  还想能跑 Movable demo；Phase 5 加 AtlasRegistry 时要重新决——要么
  全 atlas 失败一律 exit，要么补 fallback purple/black checker texture
  让缺哪张明显可见。
- **没 shader / shadow / draw_steps**：lua 的 Sprite 还有多 pass shader
  （foil / holo / dissolve）+ shadow 偏移 + `draw_from`（借别的 obj 的
  transform）。整段 Phase 7 才接，那时候才需要 GLSL 330 port。
- **没 animation_atli**：`sprite_pos.v` 存在时随机抽帧的逻辑（blind
  chips / shop sign）defer 到 Phase 7+。
- **Sprite 不持 children**：lua Sprite 有 `children` 表（譬如 Card
  下挂 4 个 Sprite：底色 / 数字 / 花色 / 印章）。Phase 5 写 Card 时
  再决定是 entt 关系还是 owning vector——MVP 单 Sprite 不需要。
