---
source: src/game/{card,cardarea}.{h,cpp}, ref-balatro/cardarea.lua:615-883
---

# CardArea + Card

Phase 5 的内容。Sprite 已经能把一张图摆到任意位置了；现在要"一手牌"
——8 张卡铺成扇形，加 / 减卡时其它卡自动平滑挪位。

## 1 · 问题：不是 (x,y) 数组，是每帧公式

第一感是开个 `std::array<Vector2, 8> slot_positions` 存死每个槽位
坐标。问题：n=4 时哪 4 个槽位用？n=10 怎么办？slot 间距随 n 变还是
不变？要么写一堆 if，要么加表，状态翻倍。

Balatro 的解法是"槽位不存在，每帧算"：`align_cards` 跑一个 lerp，
按 `(k, n, temp_limit)` 直接出 `(x, y, r)`，写到 `card.T`。
Movable 的 ease 自然把视觉位置 VT 拉过去，**插值是免费的**——你
只管目标，过渡 Phase 3 已经处理了。

## 2 · CardArea：薄壳 + 公式

字段就三类——pixel 矩形、公式参数、卡列表：

**Listing 1**: `src/game/cardarea.h`

```cpp
enum class CardAreaType { Hand, Play };

class CardArea {
 public:
  CardArea(float x, float y, float w, float h,
           CardAreaType type, float card_w, int temp_limit);

  Card* Emplace(std::unique_ptr<Card> card);
  std::unique_ptr<Card> RemoveBack();

  void Tick(float dt, float real_time);
  void HardSetCards(float real_time);
  void SetBounds(float x, float y, float w, float h);
  void Render();

  size_t Size() const { return cards_.size(); }
  Card* At(size_t i) { return cards_[i].get(); }

 private:
  void AlignCards(float real_time);

  float x_, y_, w_, h_;
  CardAreaType type_;
  float card_w_;       // canonical slot width
  int temp_limit_;     // reserves slot room when n < temp_limit
  std::vector<std::unique_ptr<Card>> cards_;
};
```

几个判断：

- **CardArea 不继承 Movable**——MVP 内 area 自己不动（不会缩、不会
  juice），省一层。Phase 6+ 真要"area 抖一下"再 promote。
- **`std::vector<std::unique_ptr<Card>>` 自有 cards**——lua 原版
  cards 在全局 `G.I.CARD` 池里，CardArea 只持非拥有引用，因为卡可
  以从 deck 转 hand 转 play。MVP 没跨 area 转移，自有更简单；真要转
  时 `RemoveBack()` 已经返 `unique_ptr`，挪到目标 area `Emplace` 就
  完了，不需要重写所有权。
- **`temp_limit_`** = lua `config.temp_limit`：n < temp_limit 时
  扇形按 temp_limit 张的间距留位，cards 居中收缩。不留位的话减卡时
  剩下的全往左堆。

## 3 · Card：MVP 阶段的薄壳

```cpp
class Card : public engine::Sprite {
 public:
  Card(float x, float y, float w, float h, const engine::Atlas& atlas,
       int sprite_pos_x, int sprite_pos_y);
};
```

字面意义上就是一个具名 Sprite。MVP 不需要 rank / suit / center /
back 这些——Phase 6 解析 `Card_Tables.lua` 时再长出来。先把 type
立起来，CardArea 才有具体类型可以装。

## 4 · AlignCards：lua 公式直译

`cardarea.lua:692-722` 是 Hand 段、`787-810` 是 Play 段。共用前缀
是 x slot 的 lerp，往下 Hand 多两个 sin wobble + 一个 y bow，Play
直接平铺。

**Listing 2**: `src/game/cardarea.cpp::AlignCards`

```cpp
void CardArea::AlignCards(float t) {
  const int n = static_cast<int>(cards_.size());
  if (n == 0) return;

  const int M = std::max(n, temp_limit_);
  const float Mm1 = static_cast<float>(std::max(M - 1, 1));
  const float nf = static_cast<float>(n);

  for (int idx = 0; idx < n; ++idx) {
    const int k = idx + 1;            // 1-based 跟 lua 对齐
    Card* c = cards_[idx].get();
    const float kf = static_cast<float>(k);

    // 共享 x slot：(k-1)/(M-1) 把 k 映到 [0,1]，加上
    // -0.5*(n-M)/(M-1) 修正在 n<M 时把整组居中。
    const float lerp_x = (kf - 1.0f) / Mm1
                       - 0.5f * (nf - M) / Mm1;
    const float slot_x = x_ + (w_ - card_w_) * lerp_x
                       + 0.5f * (card_w_ - c->T().w);

    if (type_ == CardAreaType::Hand) {
      c->T().r = 0.2f * (-nf * 0.5f - 0.5f + kf) / nf
               + 0.02f * std::sin(2.0f * t + c->T().x);
      c->T().x = slot_x;

      const float bow = std::fabs(
          0.5f * (-nf * 0.5f + kf - 0.5f) / nf);  // 0..0.25
      c->T().y = y_ + h_ * 0.5f - c->T().h * 0.5f
               - bow * c->T().h * 0.4f
               + 0.03f * c->T().h * std::sin(0.666f * t
                                             + c->T().x);
    } else {
      c->T().r = 0.0f;
      c->T().x = slot_x;
      c->T().y = y_ + h_ * 0.5f - c->T().h * 0.5f;
    }
  }
}
```

逐项对应 02 笔记 §2.1 那张表：

| 项 | 数值 | 含义 |
|--|--|--|
| `0.2 * (...) / n` | 端点 ±0.1 rad ≈ ±5.7° | 扇形旋转 |
| `0.02 * sin(2t + x)` | ±0.02 rad | r 抖动；相位用 `card.T.x` |
| `bow = abs(...)` | 0..0.25 | 弧高系数：端点大、中间 0 |
| `0.03 * sin(0.666t + x)` | ±0.03 \* card_h | y 抖动 |

## 5 · y-bow：从 game unit 翻译到 pixel

lua 原文是 `+ abs(...) - 0.2`，单位是 game unit（≈ 1/G.TILESIZE
像素）。在 G.TILESIZE=32 下整段 y bow 范围 ~0.19 game unit ≈ 3 px
——肉眼几乎看不见，扇形主要靠旋转。

我们的 `T` 直接是像素，照抄就 3 px 弧高，扁得没扇形味。所以这一步
做了个**有意识的放大**：

```cpp
- bow * c->T().h * 0.4f       // bow 范围 [0, 0.25]
                              // → 弧高 [0, 0.1*card_h] ≈ ±19 px
```

380 px 高的卡里，端点比中间低 ~10 px。视觉上明显有"中间凸起"
的扇形感，又不至于太夸张。

> **手感常数**：`0.4` 是调出来的，不是抄的。改大弧太陡，改小回
> 平铺。Phase 5 后续如果接了 G.TILESCALE / G.TILESIZE 那一层，
> 再回头用 lua 原值看效果——但目前 pixel-direct 路线里这是
> 必要的偏离。

## 6 · Tick：先 align，再 ease

```cpp
void CardArea::Tick(float dt, float real_time) {
  AlignCards(real_time);
  for (auto& c : cards_) c->Move(dt);
}
```

顺序跟 lua `Game:update` 一样：先 `align_cards` 写 T，再 Movable
的 `move` 把 VT 往 T 拉。颠倒会导致 ease 用上一帧的 T——n 改变
那一帧，VT 会先跑去旧 slot，下一帧才回正，视觉上抖一下。

`real_time` 单独传一个参数（不是用 `dt` 累加内部计时）：因为 sin
wobble 需要的是绝对相位，跟 dt 无关；调用方传游戏时间或真实时间都
行，但 idle wobble 不该跟 pause / slow-mo 一起停。

## 7 · 接进 demo：8 张 joker 一手牌

GameLayer 把 Sprite demo 整段换掉。hand area 钉在 viewport 底部
中间，宽度 `viewport_w * 0.6`（4K 高分屏自然铺开，1080p 也能塞 8
张靠重叠展示扇形）。

**Listing 3**: `src/game_layer.cpp::OnAttach` 关键几行

```cpp
const HandLayout init = ComputeHandLayout(target_w_, target_h_);
hand_.emplace(init.x, init.y, init.w, init.h,
              game::CardAreaType::Hand, kCardW, kHandTempLimit);

if (const engine::Atlas* joker = atlases_.Find("Joker")) {
  const float spawn_x = init.x + init.w;  // 右边缘
  const float spawn_y = init.y;
  for (int i = 0; i < 5; ++i) {
    hand_->Emplace(MakeJokerCard(*joker, next_sprite_idx_++,
                                 spawn_x, spawn_y));
  }
  hand_->HardSetCards(0.0f);
}
```

两个细节解决"启动时全飞一遍"：

- **spawn 落在 hand 右边缘**而不是 `(0, 0)`——后者会让 VT 从
  viewport 左上角起 ease 几百像素，肉眼上是慢吞吞从左上飞下来。
  右边缘等于"从 deck 发牌"的视觉。
- **`HardSetCards(0.0f)`** 在初始填卡后调一次，把 VT 直接 snap
  到 slot——开场 = 已发完的状态，不要看 5 张卡同时飞进来。

OnUpdate 里的输入跟 hand 维护：

**Listing 4**: `src/game_layer.cpp::OnUpdate` 关键几行

```cpp
const HandLayout layout = ComputeHandLayout(target_w_, target_h_);
hand_->SetBounds(layout.x, layout.y, layout.w, layout.h);

if (IsKeyPressed(KEY_N) && hand_->Size() < kHandSoftCap) {
  if (const engine::Atlas* joker = atlases_.Find("Joker")) {
    const float spawn_x = layout.x + layout.w;
    const float spawn_y = layout.y;
    hand_->Emplace(MakeJokerCard(*joker, next_sprite_idx_++,
                                 spawn_x, spawn_y));
  }
}
if (IsKeyPressed(KEY_M) && hand_->Size() > 0) {
  hand_->RemoveBack();
}

hand_->Tick(dt, time_);
```

`SetBounds` 每帧重设——让 viewport 面板 resize 时 hand 跟着重排。
Add / Remove 之间没需要任何"重排动画"代码：把 vector 改了下一帧
`AlignCards` 自然算出新 slot，每张卡的 T 变了，Movable ease 把 VT
平滑过去——这就是 Phase 3 那套 T/VT 分离的红利。

跑起来：5 张 joker 已在 slot 里；N 加一张从右边滑入、其它 4 张往
左让位；M 弹掉最右一张、剩下的回去填空。8 张满载时弧度最深。

## 8 · 鼠标接入：hover / click

Phase 5 polish 把 K 键退役，改成"鼠标 hover 换 cursor，左键点切
highlighted"。三件事要做：坐标换算、hit-test、ImGui 与 raylib 的输入
分工。

### 8.1 坐标换算

scene 画在 RenderTexture2D，再被 `ImGui::Image` 显示到 Viewport
面板里。鼠标点击的是 ImGui 的全局坐标，要换到 RT 像素坐标才能跟
卡对上。

```cpp
ImGui::Image(tex_id, avail, ImVec2(0, 1), ImVec2(1, 0));

const ImVec2 image_min = ImGui::GetItemRectMin();
const ImVec2 m = ImGui::GetMousePos();
const Vector2 mouse_rt{m.x - image_min.x, m.y - image_min.y};
```

`(0,1) (1,0)` 是 V 翻转——raylib FBO 在 GL texture 里上下颠倒，
ImGui 默认 UV `(0,0)` 在顶。flip 之后**鼠标坐标不需要再翻**：raylib
的 RT 用 top-left 原点（`BeginTextureMode` 里 `DrawLine(0,0,...)`
画的就是左上角），ImGui 也是 top-left；中间那一步 V flip 正好把
两套的差互相抵消。所以 `mouse_in_rt = mouse_global - image_min`，
没有 y 取负。

### 8.2 旋转矩形 hit-test

每张卡的视觉位置是 `VT`（不是 `T`）——因为扇形旋转、juice 时缩放
都写在 VT 上。lua 用 `CT = VT` 同步，我们直接读 `VT()`。

旋转矩形的 hit-test 经典做法：把鼠标点反向旋转回卡的本地坐标系
（轴对齐），再做 AABB 检查。

**Listing 5**: `src/game/cardarea.cpp` 内部辅助

```cpp
bool HitRotatedRect(const engine::Transform& vt, Vector2 p) {
  const float cx = vt.x + vt.w * 0.5f;
  const float cy = vt.y + vt.h * 0.5f;
  const float dx = p.x - cx;
  const float dy = p.y - cy;
  // -vt.r 反向旋转：把 p 变换到"卡是水平"的局部坐标系。
  const float c = std::cos(-vt.r);
  const float s = std::sin(-vt.r);
  const float lx = dx * c - dy * s;
  const float ly = dx * s + dy * c;
  // VT.scale 同时缩放命中矩形——juice 缩 VT 时点击区也跟着缩。
  const float hw = vt.w * vt.scale * 0.5f;
  const float hh = vt.h * vt.scale * 0.5f;
  return std::fabs(lx) <= hw && std::fabs(ly) <= hh;
}
```

`FindHovered` 用反向迭代——后画的卡视觉上在上面，hit 也优先：

```cpp
Card* CardArea::FindHovered(Vector2 mouse) const {
  for (auto it = cards_.rbegin(); it != cards_.rend(); ++it) {
    if (HitRotatedRect((*it)->VT(), mouse)) return it->get();
  }
  return nullptr;
}
```

MVP 还没有"hover 时把卡提到最上层"的 z-order，所以 Render 顺序
== hit 优先级。Phase 6+ 真要 z-shuffle 时，hit-test 也要跟着改用
"实际渲染顺序"——但现在两者一致。

### 8.3 ImGui 与 raylib 的输入分工

光是 raylib `IsMouseButtonPressed` 不够——鼠标可能在菜单栏、可能
在 Themes 面板，那些 click 不该穿透到 hand。所以输入门控用 ImGui
的 `IsItemHovered()`：仅当鼠标在 Image 上时才走 hit-test。

```cpp
const bool image_hovered = ImGui::IsItemHovered();
if (image_hovered && hand_) {
  if (game::Card* hit = hand_->FindHovered(mouse_rt)) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      hit->SetHighlighted(!hit->Highlighted());
      hit->JuiceUp(0.4f, 0.0f);
    }
  }
}
```

整段挂在 `DrawViewportPanel`（OnImGuiRender 阶段）。Game::Render
的顺序是：`OnRender` → `ImGui::NewFrame` → `OnImGuiRender`。
input 检测在 OnImGuiRender 里、`hand_->Tick` 在 OnUpdate（下一帧
开头）。**1 帧延迟**——点击的高亮 lift 下一帧才反应到屏幕，肉眼
感知不到。

### 8.4 highlighted 在 AlignCards 里的 lift

最后一步：选中的卡视觉拔高。lua 的 `- highlight_height` 直接加到
y 公式里；我们一样：

```cpp
const auto& tune = engine::tuning::hand;
const float lift = c->Highlighted() ? tune.highlight_lift : 0.0f;
c->T().y = y_ + h_ * 0.5f - c->T().h * 0.5f
         - lift                              // ← 选中拔高
         + bow * c->T().h * tune.bow_factor
         + 0.03f * c->T().h * std::sin(...);
```

`tune.highlight_lift` 是 `engine::tuning::hand` 里的字段（默认 40 px，
Settings 面板可调，详见 `project/tuning.md`）。lua 用 `G.HIGHLIGHT_H`
那个 game-unit 量，我们直接像素值——跟 §5 的 bow 翻译同理。再次的
偏离原作但视觉上需要的常数 taste 改造。

> **运行验证**：跑 `./build/card.exe` 进 Viewport，鼠标移到卡上
> 看 cursor 变手；左键点击单张卡看 lift +40 px 同时 squash &
> stretch；多张卡可以同时高亮（再点取消）；点空白处不变；点菜单
> 栏 / Themes 面板不会误触卡。

## 9 · 拖拽：drag controller

click 之上再加一档：按住卡拖到任意位置，松手回 slot；拖过邻居时
邻居自动让位重排。lua 的 `states.drag` 那一套——MVP 内基本算
"hand 内重排序" 用的，跨 area 转移留给 Phase 6 出牌时一起接。

四件事：跟手、不让 align 覆盖、跨邻居换位、画在最上层。

### 9.1 CardArea 持 drag 状态

```cpp
class CardArea {
  // ...
  void StartDrag(Card* card, Vector2 mouse);
  void UpdateDrag(Vector2 mouse);
  void StopDrag();
  bool IsDragging() const { return dragged_ != nullptr; }

 private:
  Card* dragged_ = nullptr;
  Vector2 drag_offset_{};   // mouse - card.T at click
  Vector2 drag_mouse_{};    // last mouse pos in RT coords
};
```

`drag_offset_` 在 StartDrag 时定下来：`offset = mouse - card.T`。
之后 UpdateDrag 只更新 `drag_mouse_`，Tick 再算 `card.T = mouse -
offset`——这样卡的"被抓住的那个点"始终贴在光标上（不是卡的 0,0
角，也不是中心）。

### 9.2 AlignCards 跳过 dragged

drag controller 要独占 dragged 卡的 T，align 公式就得让位：

```cpp
for (int idx = 0; idx < n; ++idx) {
  Card* c = cards_[idx].get();
  if (c == dragged_) continue;  // drag 控制 T，align 不写
  // ... 公式 ...
}
```

### 9.3 Tick：写 T，snap VT，重排 vector

```cpp
void CardArea::Tick(float dt, float real_time) {
  AlignCards(real_time);              // ① 写非 dragged 的 T

  if (dragged_ != nullptr) {
    const float tx = drag_mouse_.x - drag_offset_.x;
    const float ty = drag_mouse_.y - drag_offset_.y;
    dragged_->T().x = tx;
    dragged_->T().y = ty;
    dragged_->T().r = 0.0f;           // ② 握住的卡水平
    // ③ snap VT.x/y 到 T——卡贴住光标无延迟
    dragged_->VT().x = tx;
    dragged_->VT().y = ty;
    // ④ 按视觉 x 排序，dragged 越邻居 vector 索引就跟着换
    std::stable_sort(
        cards_.begin(), cards_.end(),
        [](const auto& a, const auto& b) {
          return a->T().x < b->T().x;
        });
  }

  for (auto& c : cards_) c->Move(dt);  // ⑤ ease (dragged: T==VT 等于 noop)
}
```

逐项理由：

- **② r=0 但只写 T.r**——VT.r 不 snap。这样从 fan tilt（譬如 0.1
  rad）到 0 是 ease 出来的，"抓起卡时它平下来"有过渡。如果同时
  snap 了 VT.r，鼠标按下那一帧卡会突跳到水平。
- **③ snap VT.x/y**——这是关键。Movable 的 ease 常数是为"settle
  into slot"调的，VT 滞后 T 几十 ms。drag 时几十 ms 的滞后等于卡
  在跟着鼠标"游泳"。直接写 `VT().x = T.x` 强制无延迟。release 后
  AlignCards 重新接管 dragged 卡的 T，ease 平滑回 slot。
- **④ stable_sort by T.x**——dragged 跟着鼠标，T.x 落在哪两个邻
  居中间就排到那。下一帧 AlignCards 用**新的 vector 索引**给非
  dragged 邻居算 slot，邻居 ease 让位（这是免费动画——T 变了
  Movable 自然平滑过去，不需要写"邻居重排"代码）。`stable_sort`
  避免相同 x 时的 jitter（譬如 dragged 和邻居恰好同 x 一帧）。
- **⑤ Move(dt) 仍调用 dragged**——不省这一行因为：① VT.r / VT.scale
  还在 ease（fan tilt 平下来 / juice 余响）；② T==VT 时 MoveXY
  里的 `need_x` 检查会快路径返回，性能无损。

### 9.4 Render：dragged 最后画

vector 顺序 = 画顺序 = z-order。被拖的卡如果不强制画在最后，被
旁边的卡盖住一半（特别是 stable_sort 把它挪到中间索引时）。

```cpp
void CardArea::Render() {
  for (auto& c : cards_) {
    if (c.get() != dragged_) c->Render();
  }
  if (dragged_ != nullptr) dragged_->Render();
}
```

### 9.5 GameLayer：click 还是 drag？

按下时不知道是 click 还是 drag——要等 mouse 移动超过 ImGui 内置
阈值（默认 ~6 px）才认定 drag。所以三态：

| 状态 | 触发 | 动作 |
|--|--|--|
| 候选 | mouse-down 在卡上 | 记 `pressed_card_` + `pressed_origin_rt_` |
| 升级到 drag | `IsMouseDragging(0)` 返回 true | StartDrag(候选, origin)，进入 drag 闭环 |
| 释放 | `IsMouseReleased(0)` | dragging? StopDrag : 把候选当 click 处理（toggle highlight + JuiceUp）|

```cpp
if (image_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
  pressed_card_ = hand_->FindHovered(mouse_rt);
  pressed_origin_rt_ = mouse_rt;
}

if (pressed_card_ != nullptr) {
  if (!hand_->IsDragging() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    hand_->StartDrag(pressed_card_, pressed_origin_rt_);
  }
  if (hand_->IsDragging()) {
    hand_->UpdateDrag(mouse_rt);
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (hand_->IsDragging()) {
      hand_->StopDrag();
    } else {
      pressed_card_->SetHighlighted(!pressed_card_->Highlighted());
      pressed_card_->JuiceUp(0.4f, 0.0f);
    }
    pressed_card_ = nullptr;
  }
}
```

注意：进入 drag 后 ImGui 自动把 mouse 当作"按住状态"——即使光标
离开了 viewport image，`IsMouseDragging` / `IsMouseReleased` 仍然
能检测到。所以拖卡甩到菜单栏外面再松手也能正确收尾。

> **运行验证**：跑 `./build/card.exe` 进 Viewport，按住一张卡拖
> 来回——卡跟手无延迟（snap）、邻居 ease 让位、释放后回到当前
> mouse-x 对应的 slot；甩出 viewport 释放也能正确回 slot；快速点
> 一下不进入 drag，只 toggle highlight。

## Caveats

- **没跨 area drag**：MVP 内 drag 只在 hand 自己里面重排。lua 的
  drag 可以从 hand 拖到 play area 或反过来——Phase 6 接出牌时把
  "drop target" 那一套加进来，CardArea 之间的 unique_ptr 移交走
  我们已留好的 `RemoveBack()` 接口（只是 release 时根据 mouse 位
  置选目的 area 而已）。
- **没 z-order**：Render 走 vector 顺序，左到右一层一层覆盖；只有
  dragged 单独画在最后。lua 的 hover 也提 z（hovered card 单独最
  上层），我们的 hover 不提 z——MVP 内不强求。要做就再加个
  `hovered_` 字段，Render 跟 dragged 一样的处理。
- **`-0.2` 那个常数没翻译**：lua 公式末尾的 `- 0.2` 是 "整体下偏"
  ——补"中间高、两端略低于上沿" 的视觉。我们的 bow 公式中心化到 0
  了（`bow * card_h * tune.bow_factor` 直接对称），没引入额外下偏。
  highlighted lift 单独一项加在前面，跟 bow 不耦合。
- **drag-from-deck 视觉缺失**：spawn 在 hand 右边缘是 cheap trick
  ——真正 Balatro 是从屏幕外的 deck 堆顶飞过来。Phase 6 deck area
  起来后再换成"从 deck.T.{x,y} 起飞"。
- **drag 启动阈值靠 ImGui**：`IsMouseDragging` 用 `io.MouseDragThreshold`
  默认 ~6 px。手感不够再开个 tuning 暴露——大概率不需要。
