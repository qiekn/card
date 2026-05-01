---
source: ref-balatro/engine/{object,node,moveable}.lua
---

# 01 · Object / Node / Moveable

Balatro 的引擎抽象只有三层，从下到上：`Object → Node → Moveable`。
其它所有可视/可移动东西（`Sprite`, `CardArea`, `Card`, `Particles`, `UIBox`）都从 `Moveable` 派生。

## 1 · Object（37 行的 OOP 基类）

`engine/object.lua`，注释里写明从 SNKRX (MIT) 拷的。Lua 没有真正的 class，所以作者用 metatable 链模拟出 `extend / is / __call(init)`：

```lua
function Object:extend()
  local cls = {}
  for k, v in pairs(self) do
    if k:find("__") == 1 then cls[k] = v end
  end
  cls.__index = cls
  cls.super = self
  setmetatable(cls, self)
  return cls
end

function Object:__call(...)
  local o = setmetatable({}, self)
  o:init(...)
  return o
end
```

调用 `Foo()` 等价于 `local o = setmetatable({}, Foo); o:init(...)`。**C++ 端不需要这层**——直接写 class + virtual。`super` 字段对应 C++ 的基类访问。

## 2 · Node（带 transform 的场景节点）

`engine/node.lua`. 关键字段（构造时建立）：

```lua
self.T = { x, y, w, h, r, scale }       -- 游戏单位坐标
self.CT = self.T                         -- 碰撞 transform（默认同 T）
self.click_offset / self.hover_offset    -- 拖拽偏移 + 3D shader 用
self.container = args.container or G.ROOM   -- 父参考系，默认 G.ROOM
self.children = {}                       -- 树形子节点
self.states = {                          -- 可控行为开关
  visible = true,
  collide = {can=false, is=false},
  focus   = {can=false, is=false},
  hover   = {can=true,  is=false},
  click   = {can=true,  is=false},
  drag    = {can=true,  is=false},
  release_on = {can=true, is=false},
}
self.ID = G.ID; G.ID = G.ID + 1          -- 全局唯一递增 ID
```

每个 Node 自动登记到 `G.I.NODE` 和 `G.STAGE_OBJECTS[G.STAGE]`，方便 stage 切换时整批清理。

### Container vs children
- `container` 是**渲染参考系**（Balatro 全屏只有一个 `G.ROOM`，shake 时整体偏移）。
- `children` 是树形附属关系（draw 时递归）。
- 两者**不一定相同**：一张 `Card` 的 `container` 是 `G.ROOM`，但它的 `children = {shadow, front, back, center}`。

### Hit test
`collides_with_point(point)` 做 AABB（先把 point 反向应用 container 的 translation/rotation，再判断是否落在 `T.x..T.x+T.w` × `T.y..T.y+T.h` 内）。`G.COLLISION_BUFFER` 给 hover 状态留一点松弛。

### Draw
```lua
function Node:draw()
  self:draw_boundingrect()
  if self.states.visible then
    add_to_drawhash(self)
    for _,v in pairs(self.children) do v:draw() end
  end
end
```
`add_to_drawhash` 是优化用的空间哈希，方便 hover/click 时快速反查。

### 事件钩子（默认空，子类覆盖）
```lua
function Node:click() end
function Node:release(dragged) end
function Node:animate() end
function Node:update(dt) end
function Node:hover() end       -- Controller 检测到 hover 时调用，可弹 popup
function Node:drag() end
```

## 3 · Moveable（**T/VT 双 transform 是 Balatro 的灵魂**）

`engine/moveable.lua`. 业务代码只设置 `T`，引擎每帧让 `VT` 平滑追上 `T`。

### 字段
```lua
self.T  = { x, y, w, h, r, scale }    -- 目标 transform（瞬时设置）
self.VT = { x, y, w, h, r, scale }    -- 可见 transform（每帧 ease）
self.velocity = { x, y, r, scale, mag }
self.role = {
  role_type = "Major",                -- Major | Minor | Glued
  major     = nil,                    -- 当 Minor 时指 Major
  offset    = { x = 0, y = 0 },
  xy_bond    = "Strong",              -- Strong = 完全继承
  wh_bond    = "Strong",              -- Weak   = 自己 ease
  r_bond     = "Strong",
  scale_bond = "Strong",
}
self.alignment = { type = "a", offset, prev_type, prev_offset }
self.juice = nil                      -- 瞬时 squash & stretch
self.pinch = { x = false, y = false } -- VT.w/h 是否往 0 收
self.shadow_parrallax = { x, y = -1.5 }
self.shadow_height = 0.2
```

### 指数 ease（核心公式，`moveable.lua:453-480`）

```lua
self.velocity.x = G.exp_times.xy * self.velocity.x
                + (1 - G.exp_times.xy) * (self.T.x - self.VT.x) * 35 * dt
self.VT.x = self.VT.x + self.velocity.x
```
- `G.exp_times.xy` 是衰减系数（接近 1 = 慢、平稳；接近 0 = 快但抖）
- 约束 `velocity.mag` 不超过 `G.exp_times.max_vel`，避免被弹太远
- 终值 snap：当 `|VT.x - T.x| < 0.01` 且 `|velocity.x| < 0.01` 时
  直接令 `VT.x = T.x`、`velocity.x = 0`，避免无限小抖

**`G.exp_times.*` 每帧重新算**（`game.lua:8181-8187`），保证 frame-rate independent：
```lua
G.exp_times.xy      = math.exp(-50  * self.real_dt)
G.exp_times.scale   = math.exp(-60  * self.real_dt)
G.exp_times.r       = math.exp(-190 * self.real_dt)  -- 旋转衰减最快
G.exp_times.max_vel = 70 * move_dt
```
即：`dt = 0` 时 `xy = 1`（不动）；`dt = 1/60` 时 `xy ≈ 0.43`（每帧把误差消减 57%）。这套常量直接抄到 C++ 即可。

旋转 / scale / w-h 各有类似但参数不同的 ease（`move_r`, `move_scale`, `move_wh`）。

### Major / Minor / Glued
- **Major**：自己驱动 VT 的 ease，正常 Movable 都是 Major。
- **Minor**：焊在某个 Major 上，T/VT 直接拷 Major 的（再加 offset），自己不算 ease。`bond` 字段控制每个分量是否拷：
  - `Strong` = 完全继承
  - `Weak` = 自己 ease（用于 Joker 视差等）
- **Glued**：每帧硬同步到 Major（最严格）。

`align_to_major()` 还有字符代码对齐：`type` 字符串里包含 `c`/`m`/`t`/`b`/`l`/`r`/`i`，分别表示 center/middle/top/bottom/left/right/inner。组合如 `"cm"` = center+middle, `"tli"` = top-left-inner。

### Juice (`juice_up`)
```lua
self.juice = { scale=0, scale_amt=0.4, r=0, r_amt=±0.24,
               start_time, end_time = start + 0.4 }
```
每帧 `move_juice` 用 `sin(50.8t) * 衰减` 注入额外 scale/旋转，0.4s 后 nil 掉。视觉上是「弹一下」效果，用于卡牌出现/打分等关键时刻。

### `Moveable:move(dt)` 调度

```text
if Glued: glue_to_major(major)             -- T = major.T 完全同步
elif Minor: move_with_major(dt)             -- 按 bond 决定哪些分量跟随
elif Major:
    move_juice(dt)                          -- 注入 juice 偏移
    move_xy(dt) → move_r(dt) → move_scale(dt) → move_wh(dt)
    calculate_parrallax()
```
有 `FRAME.MOVE` 防重入：同一帧不会被重复 move。

## 4 · 全局 indexing 集合

每个 Node 子类都在 init 末尾把 `self` 推到对应的全局表：

```lua
G.I.NODE        -- 所有 Node
G.I.MOVEABLE    -- 所有 Moveable
G.I.SPRITE      -- 所有 Sprite
G.I.CARD        -- 所有 Card
G.I.CARDAREA    -- 所有 CardArea
G.I.UIBOX       -- 所有 UIBox（不含 POPUP）
G.I.POPUP       -- 弹出 UIBox
G.MOVEABLES     -- 平铺 Moveable 数组（move 循环用）
G.STAGE_OBJECTS[stage]  -- 按 stage 分桶，stage 切换时整批 remove
```

`Game:draw()` 直接遍历这些集合（详见 `06-draw-pipeline.md`）。

## Port checklist

| Lua 字段 | 移植 | C++ 端 |
|--|--|--|
| `Object:extend / __call(init) / is` | drop | C++ 原生 class + virtual |
| `Object:super` | drop | base class access |
| `Node.T = {x,y,w,h,r,scale}` | keep | `struct Transform { float x,y,w,h,r,scale; }` |
| `Node.CT` (collision T) | keep | 同 |
| `Node.click_offset / hover_offset` | keep | 同 |
| `Node.container` | rewrite | 用 entt parent 组件或 `Movable*` 父指针 |
| `Node.children` | rewrite | `std::vector<Movable*>` 或 entt hierarchy |
| `Node.states.{visible,hover,click,drag,collide,focus,release_on}` | keep | bitmask 或 struct，结构基本不变 |
| `Node.ID` | keep | `entt::entity` 或自增 uint64 |
| `Node:collides_with_point` | keep | AABB + 反向 container transform |
| `Node:hover/drag/click/release/animate/update` | keep | virtual 钩子 |
| `Node:draw_boundingrect` | drop（debug 专用） | 用 raylib `DrawRectangleLines` 替代 |
| `Movable.T` / `VT` / `velocity` | **keep**（核心） | 同 Transform |
| `Movable.role.role_type / major / offset / xy_bond / wh_bond / r_bond / scale_bond` | **defer** | MVP 不做；Phase 5 看是否需要 |
| `Movable.alignment` (`cm`/`tli`/...) | defer | 用枚举 + offset 重写，不抄字符代码 |
| `Movable.juice` | keep | 同字段，Phase 3 实现 |
| `Movable.pinch` | keep | 同 |
| `Movable.shadow_parrallax / shadow_height` | keep | 同 |
| `Movable:move(dt)` 主循环 | **keep**（公式直接抄 `moveable.lua:453-480`） | virtual `Move(float dt)` |
| `Movable:hard_set_T / hard_set_VT` | keep | 同 |
| `Movable:juice_up / move_juice` | keep | sin 衰减公式直接抄 |
| `G.I.*` 全局集合 | rewrite | entt views，不再人工维护数组 |
| `G.STAGE_OBJECTS[stage]` | defer | 没切场景需求前不做 |
| `G.exp_times.{xy, r, scale, max_vel}` | keep | 一组 const float；公式 `math.exp(-K * dt)`，K 取 50/60/190 直接抄 |
