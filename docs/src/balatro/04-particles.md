---
source: ref-balatro/engine/particles.lua
---

# 04 · Particles

`Particles` 从 `Moveable` 派生，**单文件 207 行就是完整粒子系统**——
没有外部依赖，粒子本身是单色矩形（不是 sprite），靠数量和颜色组合出
"火焰、烟雾、星光"等视觉。

它是 Balatro 里少数没装在 `G.I.SPRITE` 里的视觉元素，反而塞进
`G.I.MOVEABLE`（行 59）——因为 draw 不走 atlas。

## 1 · Particles 字段（`engine/particles.lua:5-61`）

```lua
function Particles:init(X, Y, W, H, config)
  Moveable.init(self, X, Y, W, H)
  self.fill = config.fill                   -- 是否填满 W×H 而非点喷
  self.padding = config.padding or 0        -- attach 时与 major 的内边距
  self.timer = config.timer or 0.5          -- 每 timer 秒生成 1 个
  self.timer_type = config.timer_type or "REAL"   -- REAL / TOTAL
  self.lifespan = config.lifespan or 1      -- 单粒寿命（秒）
  self.speed = config.speed or 1            -- 初速度基值
  self.max = config.max or 1e15             -- 同时存活上限
  self.pulse_max = math.min(20, config.pulse_max or 0) -- 冷启动一次性数
  self.vel_variation = config.vel_variation or 1  -- 速度抖动幅度
  self.scale = config.scale or 1            -- 粒子最大边长
  self.colours = config.colours or { G.C.BACKGROUND.D }
  self.particles = {}
  self.fade_alpha = 0                       -- 0..1，整个发射器的淡出
  self.last_real_time = G.TIMERS[type] - timer    -- 第一帧立刻 spawn
  self.pulsed = 0                           -- 已 spawn 计数（pulse 用）
  self.states.{hover,click,collide,drag,release_on}.can = false
  table.insert(G.I.MOVEABLE, self)
end
```

| 字段 | 用途 |
|--|--|
| `fill` | true：在 W×H 内随机散布；false：原点喷发 |
| `attach` | 配置项：粒子发射器**挂到某个 major 上**（如 Joker 火焰）|
| `timer_type` | `REAL`（不受暂停 / 速度倍率影响）/ `TOTAL`（受影响）|
| `pulse_max` | 冷启动一次性发射上限（≤20，硬上限）|
| `max` | 同时存活上限；`pulse_max` 满后改靠它限流 |
| `vel_variation` | 0..1：每个粒子初速 = `speed * (var*rand + (1-var)) * 0.7` |
| `colours` | 颜色池，spawn 时 `pseudorandom_element` 抽一个 |
| `created_on_pause` | true 时即使游戏暂停也继续——title 动画用 |

### 1.1 attach（行 13-27）

```lua
self:set_alignment({ major = config.attach,
                     type = "cm", bond = "Strong" })
table.insert(self.role.major.children, self)
self.parent = self.role.major
self.T.x = major.T.x + padding
-- fill 时还会把自己 W/H 收到 major 内部
```

挂到 major 后 alignment `cm`（center+middle）+ `Strong` bond 让发射器
完全跟随 major（参考 01 笔记 §3 Major/Minor）。Joker 卡上的火焰就是这样
attach 到 Joker 实例的。

### 1.2 initialize（行 50-56）

```lua
if config.initialize then
  for i = 1, 60 do
    self.last_real_time = self.last_real_time - 15/60
    self:update(15/60); self:move(15/60)
  end
end
```

开场直接快进 60 帧（每步 15/60 = 0.25 s）让粒子分布"先填满"，**避免
进入场景看到空白**。背景烟雾、boss 火焰这类用。

## 2 · spawn 节奏（`particles.lua:63-107`）

```lua
while G.TIMERS[type] > self.last_real_time + self.timer
      and (#self.particles < self.max or self.pulsed < self.pulse_max)
      and added_this_frame < 20 do
  self.last_real_time = self.last_real_time + self.timer
  table.insert(self.particles, { ... })
  added_this_frame = added_this_frame + 1
  self.pulsed = self.pulsed + 1
end
```

三重限流：

1. **节拍**：每过 `timer` 秒补一颗。while 循环让低帧率下能补上欠的。
2. **总量**：`#particles < max` **或** `pulsed < pulse_max`——后者是
   "冷启动配额"，让 initialize 阶段能突破 max。
3. **单帧上限 20**：`added_this_frame < 20`，防止极慢帧率下卡死。

### 2.1 单粒 spawn 字段

```lua
{
  draw = false,                             -- move 后第一次才显示
  dir = random() * 2π,                      -- 移动方向
  facing = random() * 2π,                   -- 自身朝向（旋转）
  size = random() * 0.5 + 0.1,              -- 0.1..0.6 视觉系数（未用？）
  age = 0,
  velocity = speed * (var*rand + (1-var)) * 0.7,
  r_vel = 0.2 * (0.5 - random()),           -- ±0.1 角速度
  e_prev = 0, e_curr = 0,
  scale = 0, visible_scale = 0,
  time = G.TIMERS[type],
  colour = pseudorandom_element(self.colours),
  offset = { x, y },                        -- fill 时在 W×H 内随机
}
```

`fill + abs(T.r) < 0.1` 时，spawn 偏移会再叠一次发射器自身的旋转：

```lua
new_offset = {
  x = sin(T.r)*offset.y + cos(T.r)*offset.x,
  y = sin(T.r)*offset.x + cos(T.r)*offset.y,
}
```

注意是 `sin(r)*offset.y + cos(r)*offset.x`——**第一行不是标准 2D 旋转**
（标准是 `cos*x - sin*y`）。这是源码原样，可能利用了"r 很小时 sin≈0"
让它差不多近似旋转。移植时**保留这个怪式子**，否则视觉会偏。

## 3 · 单粒 move（`particles.lua:109-159`）

### 3.1 scale 三角包络

```lua
particle.e_curr = math.min(
  2 * math.min(
        (age / lifespan) * scale,
        scale * ((lifespan - age) / lifespan)
      ),
  scale
)
```

含义：

- `age = 0..lifespan/2`：`e_curr` 从 0 线性升到 `scale`（前半段斜率 2）
- `age = lifespan/2..lifespan`：从 `scale` 线性降到 0
- 外层 `min(..., scale)` 是封顶

**整体是一个三角脉冲包络**：粒子从 0 长大到 scale，再缩回 0 然后被移除。

### 3.2 e_vel ease（指数追赶）

```lua
particle.e_vel = (e_curr - e_prev) * scale * dt
               + (1 - scale * dt) * particle.e_vel
particle.scale = particle.scale + particle.e_vel
```

形式与 01 笔记的 Moveable exp ease 同构（`xy = exp(-K*dt)`），但 K 用了
`scale`。意思：**粒子的渲染 scale 不是直接等于 e_curr，而是用 e_vel 平滑
追赶 e_curr**——形成"放大有冲量、缩小有惯性"的视觉。

之后还有一行：

```lua
particle.scale = math.min(2 * math.min(...), scale)  -- 同 e_curr 公式
```

把 scale 再 clamp 到三角包络上限。`scale < 0` 时 `table.remove`，粒子寿
命到。

### 3.3 位置 / 旋转

```lua
offset.x += velocity * sin(dir) * dt          -- 注意是 sin
offset.y += velocity * cos(dir) * dt          -- 注意是 cos
facing  += r_vel * dt
velocity = max(0, velocity - velocity*0.07*dt)
```

两点反直觉：

- `sin → x, cos → y` 是 love2d 视图坐标系（y 向下），**不要"修正"**。
- `velocity *= (1 - 0.07*dt)`：每秒衰减 7%——粒子越飘越慢，常数 `0.07`
  抄进 C++。

`Moveable.move(self, dt)` 在最前调一次（行 114），让发射器整体本身也走
T/VT ease（attach 时跟随 major）。

## 4 · fade（`particles.lua:161-172`）

```lua
function Particles:fade(delay, to)
  G.E_MANAGER:add_event(Event({
    trigger = "ease", timer = self.timer_type,
    ref_value = "fade_alpha", ref_table = self,
    ease_to = to or 1, delay = delay,
  }))
end
```

不直接改 `fade_alpha`，而是丢给 `E_MANAGER` 做 ease 事件
（"在 `delay` 秒内把 `self.fade_alpha` ease 到 `to`"）。这是 Balatro 的
通用动画机制——任何字段都能这样 ease，**不需要每个类都写自己的过渡逻辑**。

draw 时 `colour.a *= alpha * (1 - fade_alpha)`，整个发射器淡出。

## 5 · draw（`particles.lua:174-193`）

```lua
function Particles:draw(alpha)
  prep_draw(self, 1)
  love.graphics.translate(self.T.w/2, self.T.h/2)
  for k, v in pairs(self.particles) do
    if v.draw then
      love.graphics.push()
      love.graphics.setColor(v.colour[1], v.colour[2], v.colour[3],
                             v.colour[4] * alpha * (1 - self.fade_alpha))
      love.graphics.translate(v.offset.x, v.offset.y)
      love.graphics.rotate(v.facing)
      love.graphics.rectangle("fill",
                              -v.scale/2, -v.scale/2, v.scale, v.scale)
      love.graphics.pop()
    end
  end
  love.graphics.pop()
end
```

每个粒子就是一个**居中、旋转 facing、边长 scale 的实心方块**——没贴图、
没 shader、没 outline。复杂效果纯靠数量 + colour 组合。

## Port checklist

| Lua | 移植 | C++ 端 |
|--|--|--|
| `Particles:init` 配置驱动 | keep | `struct ParticleConfig { ... }` 直译；spawn API 用 builder 或 designated init |
| `attach`（挂到 major + alignment `cm`） | rewrite | `Particles* p = new Particles(major, ...);` 让父子关系显式 |
| `padding` / `fill` | keep | 同 |
| `created_on_pause` | defer | 暂停态没起之前不需要 |
| `timer_type` (`REAL` / `TOTAL`) | keep | 两个 timer 源；`G.SPEEDFACTOR` 影响只对 TOTAL |
| `pulse_max` ≤ 20 + `pulsed` 计数 | keep | 同字段；硬上限保留 |
| `max` 同时存活 | keep | 同 |
| `vel_variation` | keep | 公式 `speed * (var*rand + (1-var)) * 0.7` 抄 |
| `initialize` 快进 60 帧 | keep | 写成 `WarmUp()` 私有函数；步长 `15/60` 保留 |
| spawn 单帧上限 20 | **keep** | 防低帧率灾难性堆积，常数保留 |
| `fill && abs(T.r) < 0.1` 那段奇怪旋转 | **keep**（公式逐字抄） | sin/cos 第一行非标准，**不要改写为标准旋转矩阵** |
| 粒子结构 `{dir, facing, size, age, velocity, r_vel, e_prev, e_curr, scale, visible_scale, time, colour, offset}` | keep | `struct Particle` 直译 |
| `r_vel = 0.2 * (0.5 - rand)` | keep | ±0.1 rad/s 角速度 |
| scale 三角包络公式 | **keep** | `0..lifespan/2` 升、`lifespan/2..lifespan` 降，外层 clamp |
| `e_vel = (e_curr-e_prev)*scale*dt + (1-scale*dt)*e_vel` | **keep** | 形式同 Moveable exp ease，K 取 `scale` |
| 位置更新 `sin→x, cos→y` | **keep** | love2d y 向下，不要"修正"，否则视觉错 |
| `velocity *= (1 - 0.07*dt)` | **keep** | 0.07 是 Balatro 标定常数 |
| `Particles:fade` 走 `E_MANAGER` | rewrite | C++ 端实现自己的 tween 系统（也可以延后，先直接改 fade_alpha）|
| `Particles:draw` 单色实心方块 | keep | `DrawRectanglePro` + 旋转 + 颜色；MVP 不上 sprite |
| 粒子注册到 `G.I.MOVEABLE`（不是 SPRITE） | rewrite | entt view + tag component，区分粒子 vs 普通 sprite |
| `pseudorandom_element` 颜色抽样 | keep | 用 PRNG 抽数组，用同一颗种子保证可重放 |
| `fade_alpha` 整体淡出 | keep | 同字段 |
| 发射器自身参与 `Moveable.move` | keep | 让发射器能 ease（attach 跟随 major 自然就好）|
