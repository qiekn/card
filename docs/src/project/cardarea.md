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
if (IsKeyPressed(KEY_K) && hand_->Size() > 0) {
  const int idx = GetRandomValue(
      0, static_cast<int>(hand_->Size()) - 1);
  hand_->At(static_cast<size_t>(idx))->JuiceUp(0.4f, 0.0f);
}

hand_->Tick(dt, time_);
```

`SetBounds` 每帧重设——让 viewport 面板 resize 时 hand 跟着重排。
Add / Remove 之间没需要任何"重排动画"代码：把 vector 改了下一帧
`AlignCards` 自然算出新 slot，每张卡的 T 变了，Movable ease 把 VT
平滑过去——这就是 Phase 3 那套 T/VT 分离的红利。

跑起来：5 张 joker 已在 slot 里；N 加一张从右边滑入、其它 4 张往
左让位；M 弹掉最右一张、剩下的回去填空；K 随机 squash & stretch
一张。8 张满载时弧度最深。

> **运行验证**：跑 `./build/card.exe` 进 Viewport，按 N 加到 8 张
> 看扇形最完整；按 M M M 看收缩重排；按 K 看 juice 效果不会因为
> 在扇形里就拐弯。viewport 拖大缩小，hand 跟着重新铺。

## Caveats

- **没接输入交互**：lua Card 有 hover / drag / click 三套状态机；
  MVP 只有键盘 demo。Phase 5 polish 会接 raylib `IsMouseButton*` +
  hit test（用 VT 而非 T，跟 lua 的 `CT = VT` 一致）。
- **没 z-order**：Render 走 vector 顺序，左到右一层一层覆盖。lua
  hover 时会把那张卡提到最上层；MVP 不需要 hover 所以不需要。Phase 5
  polish 加 hover 时一起处理——大概率是"hovered card 单独最后
  draw"那种最简单做法。
- **没 sort**：lua hand 末尾按视觉 x 排序 `self.cards`，drag 跨过
  邻居时立刻换位。我们没 drag 也没必要排，emplace 顺序就是 slot
  顺序。
- **`-0.2` 那个常数没翻译**：lua 公式末尾的 `- 0.2` 是 "整体下偏"
  ——补"中间高、两端略低于上沿" 的视觉。我们的 bow 公式中心化到 0
  了（`bow * card_h * 0.4` 直接对称），没引入额外下偏；如果接了
  highlight_height（选中拔高），那时再决定是 lua 的"上沿对齐 +
  bow 偏"还是我们这套"中心 + bow 对称"，两个等价但参数不同。
- **没 highlighted / highlight_height**：选中时拔高的逻辑（hand 用
  `highlight_height = G.HIGHLIGHT_H`）整段 defer 到接 click 那刀。
- **drag-from-deck 视觉缺失**：spawn 在 hand 右边缘是 cheap trick
  ——真正 Balatro 是从屏幕外的 deck 堆顶飞过来。Phase 6 deck area
  起来后再换成"从 deck.T.{x,y} 起飞"。
