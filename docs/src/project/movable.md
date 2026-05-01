---
source: src/engine/movable.{h,cpp}, ref-balatro/engine/moveable.lua:208-522
---

# Movable

Phase 3 的全部内容。一句话目标：业务代码只设"想去哪"，引擎把"现在到哪了"
平滑算出来。

## 1 · 问题：游戏代码怎么让东西动

最朴素的写法：
```cpp
card.x += 5.0f;  // 每帧推一点
```

动是动了，但是**得算"何时停"**。要 ease，得自己写 tween：起点、终点、
持续时间、当前进度、缓动曲线。每个新的"想去的位置"都得 cancel 上一个 tween。
一打就乱。

Balatro 的解法是**两个 transform**。`T`（target）瞬时设置，`VT`
（visible）每帧 ease 一步。业务代码只碰 `T`，渲染只读 `VT`。

**Listing 1**: `src/engine/movable.h` 字段

```cpp
class Movable {
 protected:
  Transform t_{};       // 目标，业务设
  Transform vt_{};      // 可见，引擎算
  Velocity velocity_{}; // ease 状态机
  std::optional<Juice> juice_;
  bool pinch_x_, pinch_y_;
  float elapsed_;
};
```

设 `T.x = 500`，下一帧 `VT.x` 不会立刻变 500——它会朝 500 走一段，
下一帧再走一段，速度由"差距 + 衰减"决定。打断也不用 cancel：再设
`T.x = 100`，`VT.x` 自然换头掉返。

## 2 · ease 公式

核心一行（`movable.cpp:108-112`）：

```cpp
velocity_.x = exp_xy * velocity_.x
            + (1 - exp_xy) * (t_.x - vt_.x) * 35 * dt;
vt_.x += velocity_.x;
```

`exp_xy` 是衰减系数，每帧重算：

```cpp
const float exp_xy = std::exp(-50.0f * dt);
```

50 是 Balatro 的口味常数（`game.lua:8181`）。直觉：

- `dt = 0` → `exp_xy = 1` → velocity 不变 → 不动
- `dt = 1/60` → `exp_xy ≈ 0.43` → 每帧把 velocity 平均化到"43% 旧的 +
  57% 新方向"
- `dt` 越大 `exp_xy` 越小 → 反应越激进

`exp(-K * dt)` 是教科书指数衰减——把"一秒衰减一半"那种 frame-rate
dependent 的写法掏空。同一台机器在 60 / 144 / 240 fps 下视觉一致。

第二个常数 `35`（`kXyGain`）是力的强度——`(T - VT)` 是位置误差，乘 35
转成 velocity 增量。也是 Balatro 标定的，照抄。

## 3 · snap：别无限小抖

数学上指数 ease 永远到不了，浮点会无限小抖动。lua 直接 if-snap
（`moveable.lua:471-478`）：

```cpp
if (std::abs(vt_.x - t_.x) < 0.01f
    && std::abs(velocity_.x) < 0.01f) {
  vt_.x = t_.x;
  velocity_.x = 0.0f;
}
```

误差和 velocity 都小于 0.01 就咬死。这一对常数管位置；旋转和 scale 各
有自己的阈值（`kSnapR = 0.001`、`kSnapScale = 0.001` —— 转一弧度比
推 1 个 game unit 显眼很多，阈值更严）。

## 4 · max_vel：别被弹太远

设 `T.x` 突然从 100 跳到 5000，第一帧 `velocity.x` 会被算出来一个
巨大值，box 会"咻"一下飞过去。lua 限了一个 magnitude 上限：

```cpp
const float max_vel = 70.0f * dt;
const float mag2 = vx*vx + vy*vy;
if (mag2 > max_vel * max_vel) {
  const float mag = std::sqrt(mag2);
  vx = max_vel * vx / mag;
  vy = max_vel * vy / mag;
}
```

`70 * dt` 也是 Balatro 标定。它把"距离巨大跳"变成"快速但有限的滑行"，
保持手感连贯。

## 5 · 旋转的 side sway

`MoveR` 有一个看着奇怪的项（`movable.cpp:171`）：

```cpp
const float sway = (dt > 0) ? (0.015f * velocity_.x / dt) : 0.0f;
const float des_r = t_.r + sway + (juice ? juice->r * 2 : 0);
```

Box 横向移动越快，目标角度被 push 越远——视觉上像跑步时身体侧倾。
0.015 是 lua 标定。我们当前 demo 里 box 不旋转所以看不到（`T.r` 一直 0），
但要 trigger juice 时 r 会被 juice 注入，能看出 sway 仍然在叠加。

## 6 · scale / w-h：两种 ease

`MoveScale` 跟 `MoveXY` 同结构，K = 60，gain 不乘 35（直接 `des - vt`）。
`MoveWH` 不同——它是**线性速度** `8 * dt * T.w`，不是 exp ease。原因：
`pinch.x = true` 时 `VT.w` 要从 `T.w` 收到 0（flip 动画），机械感比
弹性感对。

```cpp
vt_.w += 8.0f * dt * (pinch_x_ ? -1.0f : 1.0f) * t_.w;
vt_.w = std::clamp(vt_.w, 0.0f, t_.w);
```

`pinch` 的实战在 Phase 5（Card flip 时一对 `pinch.x = true → 等到 VT.w = 0
→ 切贴图 → pinch.x = false`），现在只是把 hook 留出来。

## 7 · juice：squash & stretch

`JuiceUp(0.4)` 触发 0.4 秒的弹性收缩（`movable.cpp:69`）：

```cpp
vt_.scale = 1.0f - 0.6f * amount;  // 立刻 squash
juice_ = Juice{ .scale_amt = amount,
                .start_time = elapsed_,
                .end_time = elapsed_ + 0.4f };
```

立刻把 `VT.scale` 拍下去（"被打了一拳"），再让 `MoveJuice` 每帧用 sin
注入 wobble，0.4 秒后 `juice_.reset()`：

```cpp
const float t = elapsed_ - juice->start_time;
const float fade = max(0, (juice->end_time - elapsed_) / duration);
juice->scale = juice->scale_amt * sin(50.8 * t) * fade*fade*fade;
juice->r     = juice->r_amt     * sin(40.8 * t) * fade*fade;
```

频率 50.8 / 40.8 是 Balatro 标的——一秒大概 8 / 6.5 个 wobble。fade
立方 / 平方让尾巴软着陆。

scale 进 `MoveScale` 的 `des_scale` 加成；r 进 `MoveR` 的 `des_r` 加成
（× 2）。所以 juice **不直接写 VT**，是改 ease 的目标——这样 juice
发生在移动中也能叠加上原本的位移 ease，不会打架。

## 8 · 完整调度

`Move(dt)` 顺序（`movable.cpp:76-91`）：

```cpp
elapsed_ += dt;
const float exp_xy = exp(-50 * dt);  // 每帧重算 K
const float exp_scale = exp(-60 * dt);
const float exp_r = exp(-190 * dt);  // r 衰减最快
const float max_vel = 70 * dt;

stationary_ = true;
MoveJuice();              // 先算 juice 给 scale/r 加成
MoveXY(dt, exp_xy, max_vel);
MoveR(dt, exp_r);          // sway 用 velocity_.x，要在 XY 之后
MoveScale(dt, exp_scale);
MoveWH(dt);
```

顺序**不是任意**：`MoveR` 的 sway 项读 `velocity_.x`，必须在 `MoveXY`
之后；`MoveScale` 加上 `juice.scale`，必须在 `MoveJuice` 之后。直接
照 lua 抄。

## 9 · 接进 demo

`game_layer.cpp` 加一个 `engine::Movable demo_`，1/2/3 切 `T.x`、J 触发
juice、每帧 `Move(dt)` 后用 `VT` 画方块：

```cpp
if (IsKeyPressed(KEY_ONE))   demo_.T().x = vw * 0.25f - w*0.5f;
if (IsKeyPressed(KEY_TWO))   demo_.T().x = vw * 0.50f - w*0.5f;
if (IsKeyPressed(KEY_THREE)) demo_.T().x = vw * 0.75f - w*0.5f;
if (IsKeyPressed(KEY_J))     demo_.JuiceUp(0.4f, 0.0f);
demo_.Move(dt);
```

跑起来：按 1 → box 滑到左 1/4 槽；连按 1 / 3 / 2 → box 中间不停顿、
每次换头滑过去；按 J → box 突然缩小再弹回原 scale；移动中按 J →
缩 / 弹叠加在滑行上，没有打架。

> **运行验证**：build 起来跑 `./build/card.exe` 进 Viewport 面板。
> HUD 第二行显示 `T.x` / `VT.x` / `juice` 状态——`VT.x` 数字会平滑追
> `T.x`，触发 juice 时 `juice=yes` 持续 0.4 秒。

## Caveats

- **没接 Major/Minor/Glued**（lua 的 role 系统）。MVP 全部当 Major。
  Phase 5 加 Card 的 4 个子 Sprite 时再看是否需要——Sprite 当 Glued
  锁到 Card 的 T 上是最干净的写法。
- **没 `calculate_parrallax`**：依赖 `G.ROOM`，我们没 room 概念。
  Phase 4 画 shadow 时再加。
- **没 re-entry guard**：lua 用 `FRAME.MOVE` 防同一帧 Move 多次。MVP
  集中调用一次，不需要。如果以后 hierarchy 出现（Card 触发子 Sprite Move），
  再加 frame counter。
- **`Update` / `Render` 是空 hook**：Movable 自己不画——画法由派生类
  `Sprite` 决定（Phase 4）。当前 demo 直接读 `demo_.VT()` 在 GameLayer
  里画，跳过了 hook。