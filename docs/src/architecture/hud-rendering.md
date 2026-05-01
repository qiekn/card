---
source: 替代 UIBox HUD 的 immediate-mode 方案
---

# HUD Rendering（immediate-mode）

> **结论先行**：替代 Balatro 的 UIBox HUD（05 笔记里 drop 的部分），
> C++ 端用 raylib `DrawTextEx` + `MeasureTextEx` 每帧直接绘制；总代码
> 估 < 500 行覆盖 MVP。

## 1 · MVP 阶段需要画什么

按 02 / 06 笔记 + Balatro 实际游玩界面，最小 HUD 6 个数据点：

| 字段 | 来源（C++ 端） | 位置（参考 Balatro 1080p）|
|--|--|--|
| Chips（蓝） | `currentHand.chips` | 屏幕中下，出牌区上方 |
| Mult（红） | `currentHand.mult` | 屏幕中下，紧挨 chips 右侧（× 号分隔） |
| Score（白） | `roundScore` | 屏幕中下，chips × mult 下方 |
| Target（白，蓝边） | `blind.chips` | 左下角，blind 框内 |
| Hands（蓝按钮） | `handsLeft` | 屏幕底部偏右 |
| Discards（红按钮） | `discardsLeft` | 屏幕底部偏右，hands 旁边 |

外加 6 个次要：

- Dollars（金黄）/ Ante（屏幕左下）/ Round（屏幕左下）
- Hand level（手牌评估时）/ Hand name（"Two Pair"等） / 选中卡数

整体面积 < 屏幕 20%，集中在底部。

## 2 · 渲染时机

按 06 笔记 18 阶段：HUD 应该在**阶段 6（CARDAREA 之后） + 阶段 7（CARD
之前）**——这样 HUD 不会被卡牌盖住，但会被 dragging.target / OVERLAY_MENU
盖住（符合 Balatro 实际行为）。

C++ 端 `Game::Draw()` 在原 18 阶段顺序里塞一个新阶段 6.5 `DrawHUD()` 即可，
其它阶段不动。

## 3 · 数字格式化

逐字抄 `misc_functions.lua:1201-1217`：

```lua
function number_format(num)
  if num >= 1e11 then
    -- 科学计数法 "1.234e11"
    return string.format("%.3f", x / 10^fac) .. "e" .. fac
  end
  -- 正常数字 + 千分位逗号
  -- 保留小数位规则：>=100 → 整数, >=10 → 1 位, <10 → 2 位
end
```

C++ 端：单函数 `std::string FormatNumber(double num)`，分三档实现：

| 区间 | 格式 | 示例 |
|--|--|--|
| `n >= 1e11` | `"%.3fe%d"` | `1.234e11` |
| `100 <= n < 1e11` | `"%.0f"` + 千分位逗号 | `1,234,567` |
| `10 <= n < 100` | `"%.1f"` | `99.5` |
| `n < 10` | `"%.2f"` | `7.25` |

`G.E_SWITCH_POINT = 1e11` 是 Balatro 切到 e 表示法的阈值，**逐字保留**。

## 4 · 字号自适应

逐字抄 `misc_functions.lua:1219-1231`：

```text
amt < 1e6:       scale = 0.75
1e6 <= amt < 1e11: scale = 14 * 0.75 / (floor(log(amt)) + 4)
amt >= 1e11:     scale = 0.7
```

`log` 是自然对数 `math.log`（不是 log10）。例子：

- amt = 1e6 → log ≈ 13.8 → scale = 14*0.75/(13+4) = 0.618
- amt = 1e8 → log ≈ 18.4 → scale = 14*0.75/(18+4) = 0.477
- amt = 1e10 → log ≈ 23.0 → scale = 14*0.75/(23+4) = 0.389

随数值变大字号自动缩小，避免溢出 HUD 框。**这套常数（14, 0.75, +4, 0.7）
全部抄到 C++**。

## 5 · 字体度量

raylib `MeasureTextEx(font, text, fontSize, spacing) → Vector2`（像素 w, h）。

每帧绘制前先 measure 一次拿到宽度，用来：

- 右对齐（chips 数字向左延伸）
- 框背景宽度（DrawRectangleRounded 包住数字）
- 居中

不要抄 lua 端的 `FONTSCALE` / `squish` / `TEXT_HEIGHT_SCALE` —— 那是
LÖVE 字体的多语言适配遗产，raylib 用 TTF 直接 LoadFontEx 就能拿准确度量。

字体选择（建议）：

- **m6x11plus**（Balatro 默认像素字体），免费，可在 dafont 找到
- 或 raylib 自带 `GetFontDefault()` 起步，质感差但不阻塞 Phase 6

## 6 · 弹一下动画（juice 替代）

Balatro 的 chips 数字在收到加分时会**弹一下**——这是 Movable.juice 系统
（01 笔记 §3）。C++ 端 immediate-mode HUD 没接 Movable，要单独实现一个
轻量 `HudDigit`：

每个 `HudDigit` 持有：

- `displayedValue` / `targetValue`：当前显示值 + 目标值（用 exp ease 追）
- `scaleJuice`：瞬时放大量，0..0.4
- `juiceStartTime`：juice 触发时刻

每帧 update：

- `displayedValue` 用 01 笔记的 exp ease 公式追 `targetValue`，
  **常数 50 直接抄**（`exp(-50*dt)`）
- `scaleJuice` 按 Movable.juice 衰减：`0.4 * sin(50.8*age) * (1 - age/0.4)`，
  age > 0.4 归零

每帧 draw：

- `DrawTextEx(font, FormatNumber(displayedValue), pos,
  fontSize * (baseScale + scaleJuice), spacing, color)`

收到 chips 增量时调 `JuiceUp()`：写 `juiceStartTime = GetTime()`，
`targetValue += amount`。**ease 公式 + 0.4s 周期 + sin(50.8 t) 全部抄
01 笔记的常数**。Phase 6 实现细节。

## 7 · 颜色

`globals.lua` 里 `G.C.{CHIPS, MULT, MONEY, BLUE, RED, ...}` 大概 30 种
颜色（Phase 4 时直接抄一份到 C++ const 表）。MVP 用得到的 6 个：

| C++ 名 | RGB | 用途 |
|--|--|--|
| `CHIPS_BLUE` | 大致 `(0.4, 0.6, 0.95)` | chips 数字 + 框 |
| `MULT_RED` | `(0.97, 0.45, 0.45)` | mult 数字 + 框 |
| `MONEY_GOLD` | `(0.96, 0.79, 0.30)` | dollars |
| `WHITE` | `(0.97, 0.97, 0.96)` | 一般文本 |
| `BACKGROUND_DARK` | `(0.18, 0.20, 0.22)` | HUD 框背景 |
| `OUTLINE` | `(0.0, 0.0, 0.0, 0.5)` | 描边 |

**确切 RGB 等 Phase 4 从 globals.lua 摘**——这里只列名字。

## 8 · 边框 / 圆角

raylib `DrawRectangleRounded(rect, roundness, segments, color)`：

- `roundness = 0.3`（Balatro 大致圆角度）
- `segments = 8`（够圆滑了）
- 描边可以 `DrawRectangleRoundedLines` 套外面一圈

Balatro 数字框的"凹陷"质感靠：

1. 主体 `DrawRectangleRounded(BACKGROUND_DARK)`
2. 顶部 1px `OUTLINE alpha=0.3`（高光）
3. 底部 1px `OUTLINE alpha=0.5`（阴影）

3 行 + 主体 1 行 = 4 行/框。MVP 不上"凹陷"也能用，纯单色框就行。

## 9 · Layout：单遍 immediate-mode

每帧从屏幕左下角往右排，**不需要保留态**：

- 维护一个 `cursor` 位置，画一行 advance 一段
- 左下：dollars / ante / round 三行
- 中下：chips × mult / score 两行
- 右下：hands / discards 两个按钮

**没有 layout 引擎，没有树**——全部坐标手算。Phase 4 写一遍调好就稳定，
之后改字号 / 位置都是改一行常量。

具体函数签名 + 坐标常量等到 Phase 6 实现时再确定（见项目文档 B）；这篇
笔记只锁定**架构形式**：immediate-mode + 单帧 cursor + 手算坐标。

## 10 · 与 02 笔记 Card 子 UI 的关系

02 笔记里 Card 有 4 个 UIBox 子节点（use_button / alert / focused_ui /
h_popup），它们**也走 immediate-mode**（05 笔记决议）。建议放在 `DrawHUD`
**之后、dragging 之前**画——hover popup 永远盖在 HUD 之上，但仍被拖拽
中的卡盖住。

C++ 端 `Game::Draw()` 调用顺序（细化 06 笔记的 18 阶段）：

| 新阶段 | 内容 |
|--|--|
| 6.5 | DrawHUD |
| 7 | DrawAllCards（含 Card 自己的 children Sprite）|
| 7.5 | DrawCardPopups（hover popup / alert / use_button）|
| 14 | DrawDraggingTarget |
| 18 | DrawCursor |

## 11 · 不做的事

明确**不**实现：

- 多语言字体 squish 适配（`FONTSCALE / squish / TEXT_HEIGHT_SCALE`）
- 文字 ease-in / 渐显（只动 scale juice，不动 alpha）
- 文字垂直翻转（`vert = true` 那段，UI 几乎不用）
- 动态字号梯度（`hand level` 等级越高字越大）—— Phase 6+ 再加
- Slider / Input box（HUD 不需要）

这些都是 UIBox 留下的"装饰功能"，Balatro 用得不多，移植不抵成本。
