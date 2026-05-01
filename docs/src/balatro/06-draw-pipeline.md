---
source: ref-balatro/game.lua:8328-8602
---

# 06 · Draw Pipeline

> Balatro 没有 z-buffer，**遍历顺序 = 绘制顺序 = 视觉层级**。整个
> `Game:draw()` 是一长串 for 循环，按 18 个固定阶段走完一帧。

理解这个文件就能理解：阴影为什么不会互相遮挡？拖拽中的卡为什么永远在最
上层？CRT 扫线为什么只影响游戏画面不影响 cursor？

## 1 · Canvas 三层

```text
G.CANVAS                   -- 主渲染目标，按 G.CANV_SCALE 缩放
  │  画完所有游戏内容
  ▼
G.AA_CANVAS                -- 把 CANVAS 当贴图，套 CRT shader 画到这层
  │  也按 G.CANV_SCALE
  ▼
default framebuffer        -- 把 AA_CANVAS 缩回 1/G.CANV_SCALE 再 blit
```

**两层缩放 = 内部超采样**：游戏先按 `CANV_SCALE`（通常 ≥1）渲染到大画布，
再缩回原分辨率显示——天然抗锯齿。CRT shader 接到中间层（`AA_CANVAS`）后处理，
不会被最终缩放破坏扫线频率。

`game.lua:8336-8338, 8550-8598`：

```lua
love.graphics.setCanvas({ self.CANVAS })
love.graphics.push()
love.graphics.scale(G.CANV_SCALE)
-- ...所有游戏内容画到 CANVAS...
love.graphics.pop()

love.graphics.setCanvas(G.AA_CANVAS)
G.SHADERS["CRT"]:send(...)
love.graphics.setShader(G.SHADERS["CRT"])
love.graphics.draw(self.CANVAS, 0, 0)         -- CANVAS → AA_CANVAS（套 CRT）

love.graphics.setCanvas()                     -- 默认 fb
love.graphics.setShader()
love.graphics.scale(1 / G.CANV_SCALE)
love.graphics.draw(G.AA_CANVAS, 0, 0)         -- AA_CANVAS → 屏幕
```

## 2 · Game:draw 18 阶段（`game.lua:8328-8548`）

每个阶段都是 `for k, v in pairs(集合): if 条件 then push/translate_container/draw/pop`。
**条件常含 `not v.parent`**——只画"游离"的 root 对象，子节点由父对象的
draw 递归触发。

| # | 集合 / 对象 | 条件 | 行 |
|--|--|--|--|
| 1 | `G.SPLASH_BACK` | 非空 | 8343-8352 |
| 2 | `G.I.NODE` | `not v.parent` | 8355-8362 |
| 3 | `G.I.MOVEABLE` | `not v.parent` | 8364-8371 |
| 4 | `G.SPLASH_LOGO` | 非空 | 8373-8378 |
| 5 | `G.I.UIBOX`（非 attention） | `not v.attention_text and not v.parent and v != OVERLAY_MENU/screenwipe/OVERLAY_TUTORIAL/debug_tools/online_leaderboard/achievement_notification` | 8392-8408 |
| 6 | `G.I.CARDAREA` | `not v.parent` | 8410-8417 |
| 7 | `G.I.CARD` | `not v.parent and v != dragging.target and v != focused.target` | 8419-8426 |
| 8 | `G.I.UIBOX`（attention） | `v.attention_text` | 8428-8440 |
| 9 | `G.SPLASH_FRONT` | 非空 | 8442-8447 |
| 10 | `OVERLAY_TUTORIAL` + highlights | 非空 | 8450-8468 |
| 11 | `OVERLAY_MENU` | 非空且非 dragging | 8471-8476 |
| 12 | `debug_tools` | 非空且非 dragging | 8479-8486 |
| 13 | `G.I.ALERT` | 全部 | 8489-8495 |
| 14 | `dragging.target` | 非空 | 8497-8502 |
| 15 | `focused.target Card` | 仅当不在 hand 或本身就是 dragging | 8504-8516 |
| 16 | `G.I.POPUP` | 全部 | 8518-8523 |
| 17 | `achievement_notification` | 非空 | 8525-8530 |
| 18 | `screenwipe` + `CURSOR` | 非空 | 8532-8546 |

### 2.1 关键 invariant

**`not v.parent` = "顶层对象"**：例如 `Card` 在 `G.hand` 时 `v.parent ==
G.hand`，所以阶段 7 跳过它，由阶段 6 `CardArea:draw()` 递归触发——保证
`hand` 的卡画在 `hand` 框架之上。

**dragging.target 单独画在阶段 14**：被排除出阶段 7、8、11，**最后才画
一次**——拖拽中的卡永远盖在所有 UIBox / OVERLAY_MENU 之上。这是用阶段
顺序硬保证的层级，没有 z-sort。

**focused.target Card 在阶段 15**：仅当卡**不在手牌区**才单独画到顶层
（手牌的 focus 由阶段 6 `CardArea:draw` 处理，避免出现"飞出来一张卡"的
错位）。

**POPUP 在 ALERT 之上**（阶段 13 vs 16）：详情卡 popup 永远盖在小红点之上。

### 2.2 `translate_container()`

每次 draw 之前都调用 `v:translate_container()`。它把 container（默认
`G.ROOM`）的 translate / rotate 应用到当前 graphics 栈——这就是 01 笔记里
"`G.ROOM` shake 时整体偏移"的实现：所有顶层对象都共享 `G.ROOM` 这一个
container，shake `G.ROOM.T.r` 一次性影响整屏。

## 3 · Card 内部分阶段（`card.lua:6027-`）

阶段 7 调 `Card:draw()`，**默认 `layer="both"`**，**单次调用内部**走两段：

```lua
function Card:draw(layer)
  layer = layer or "both"
  if layer == "shadow" or layer == "both" then
    -- 准备 shader uniform，画 shadow（行 6052-6072）
    G.shared_shadow:draw_shader("dissolve", self.shadow_height)
  end
  if layer == "card" or layer == "both" then
    -- 计算 tilt_var，画卡面（行 6074+）
    -- children.{back/front/center/floating_sprite}:draw_shader(...)
  end
end
```

`Game:draw()` **不会两遍遍历 Card**——这是 02 笔记需要修正的地方：阴影和
卡面的相对层级**靠 Card 内部分段 + 子 Sprite 绘制顺序保证**，不靠外层
两遍循环。

实际"shadow 不被卡面遮挡"靠的是：

1. 阶段 7 遍历卡时，**每张卡先画自己的 shadow，再画自己的卡面**
2. shadow 通过 03 笔记的 `_shadow_height` 偏移到 `shadow_parrallax` 反方向
   + 缩小（`scale *= 1 - 0.2*h`），落在比卡面低的"地板"位置

跨张卡之间**阴影确实会被相邻卡的卡面盖住**——这是 Balatro 的视觉妥协，
近距离看手牌会注意到。

### 3.1 shadow_height 取值（`card.lua:6065-6070`）

```lua
self.shadow_height = (
  ((self.highlighted and self.area == G.play) or self.states.drag.is)
    and 0.35
  or (self.area and self.area.config.type == "title_2") and 0.04
  or 0.1
)
```

- **拖拽中 / play 区高亮**：0.35（影子拖远，强调"漂浮"）
- **title_2**：0.04（很近的地板影）
- **默认**：0.1

## 4 · CRT 后处理（`game.lua:8552-8598`）

```lua
G.SHADERS["CRT"]:send(
  "distortion_fac",
  { 1.0 + 0.07 * G.SETTINGS.GRAPHICS.crt / 100,
    1.0 + 0.10 * G.SETTINGS.GRAPHICS.crt / 100 }
)
G.SHADERS["CRT"]:send("crt_intensity", 0.16 * crt / 100)
G.SHADERS["CRT"]:send("scanlines",
  G.CANVAS:getPixelHeight() * 0.75 / G.CANV_SCALE)
G.SHADERS["CRT"]:send("time", 400 + G.TIMERS.REAL)
G.SHADERS["CRT"]:send("hovering", 1)            -- CRT 一直"hover"，用来推 vertex
love.graphics.setShader(G.SHADERS["CRT"])
G.SHADERS["GRAPHICS"].crt = G.SETTINGS.GRAPHICS.crt / 0.3
```

CRT 强度由 `G.SETTINGS.GRAPHICS.crt`（百分比 0..100）控制，默认乘 `0.3`
削弱再除回去——这是为了 settings 滑块在 0..100 范围内能精细调节但视觉上
不至于过分扭曲（一个手感常数）。

`scanlines = canvasHeight * 0.75 / CANV_SCALE`：扫线密度 = 像素高度的 75%，
保证不同分辨率下扫线视觉密度一致。

## 5 · 帧时分段（`timer_checkpoint`）

8335 / 8391 / 8409 / 8547 / 8601 五个 checkpoint，按段统计耗时：

| 段 | 内容 |
|--|--|
| `start->canvas` | clear + setCanvas |
| `primatives` | NODE / MOVEABLE / SPLASH_LOGO（阶段 1-4） |
| `uiboxes` | UIBox 非 attention（阶段 5） |
| `rest` | CARDAREA / CARD / 其余阶段（阶段 6-18） |
| `canvas` | CRT + 缩回到默认 fb |

排查掉帧瓶颈时直接看哪段最慢——MVP 阶段不需要这套 instrumentation，
但保留 ↑checkpoint 名词当 profile 标记位很有用。

## Port checklist

| Lua | 移植 | C++ 端 |
|--|--|--|
| 三层 canvas（`CANVAS` → `AA_CANVAS` → 默认 fb） | keep | raylib `RenderTexture2D` × 2；`BeginTextureMode/EndTextureMode` 包住每层 |
| `G.CANV_SCALE` 内部超采样 | keep | 通常 = 2，写成可配置 const |
| `Game:draw()` 18 阶段顺序 | **keep** | 直译为单一 `Game::Draw()` 函数，每阶段一段 if/for；**保留顺序** |
| 各 `G.I.*` 集合遍历 | rewrite | entt views（同 01 笔记）；`view<Card>().each()` 等 |
| `not v.parent` 过滤 | keep | C++ 端用"hierarchy root"标志或 component；MVP 直接维护 `std::vector<Drawable*> roots` 也行 |
| `dragging.target` 单独画到阶段 14 | keep | 同思路：把 dragging 的卡从主循环排除，最后单独 Draw 一次 |
| `focused.target Card`（仅当不在 hand）单独画 | defer | 手柄聚焦 Phase 8+，先不做 |
| `OVERLAY_MENU` / `OVERLAY_TUTORIAL` / `screenwipe` 顶层 | defer | 没起 menu 系统前不需要 |
| `G.I.POPUP` 阶段 16 | rewrite | hover popup immediate-mode 画在主循环末尾即可，不需要单独集合 |
| `G.I.ALERT` 阶段 13 | defer | 没成就系统前不需要 |
| `Card:draw(layer)` 单次内部分 shadow + card | **keep** | C++ `Card::Draw()` 内部两段 if，**不要拆成两次外层循环** |
| `G.shared_shadow` 全局变量传递 | rewrite | C++ 不要全局；用本地变量传给 `DrawShadow` 函数 |
| `shadow_height` 三档取值（0.35 / 0.04 / 0.1） | keep | 常量保留，取值逻辑直译 |
| `translate_container()` 每帧给所有顶层调一次 | keep | C++ `BeginRoom() / EndRoom()` helper，把 `G.ROOM` 的 translate/rotate 应用到当前 transform 栈 |
| CRT 后处理 | defer | Phase 7+ 启用；先用空 shader 直接 blit |
| `crt_intensity = 0.16 * crt/100` | keep | settings UI 给百分比，shader 内乘系数 |
| `crt = crt * 0.3` 削弱再除回 | keep | 这是手感常数，直译 |
| `scanlines = canvasHeight * 0.75 / CANV_SCALE` | keep | 公式直译 |
| `timer_checkpoint` profile 段 | rewrite | 用 raylib `GetTime()` 或自定义 `Profiler` 类；段名沿用 |
| `_RELEASE_MODE && DEBUG && F_VERBOSE` 的 FPS / drawhash 调试绘制 | drop | C++ 端用 `DrawFPS(10, 10)` 一行就够了 |
