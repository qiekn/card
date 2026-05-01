---
source: ref-balatro/engine/ui.lua
---

# 05 · UI System（仅了解，不移植）

> **结论先行**：整个 UIBox / UIElement / `G.UIDEF` 体系**全部 drop**，
> C++ 端用 raylib immediate-mode 直接画 HUD + 必要时挂 raygui。本篇只
> 解释这套系统怎么工作、为什么不值得抄。

`engine/ui.lua` 1403 行 + `functions/UI_definitions.lua` **16918 行**——
Balatro 自造了一套**保留态声明式 UI（retained-mode declarative UI）**，
带自己的盒模型、layout 引擎、字体度量、热更新数据绑定。这是 Balatro 代码
量最大的一坨基础设施。

## 1 · 节点类型枚举（`globals.lua:478-488`）

```lua
G.UIT = {
  T = 1,    -- text
  B = 2,    -- box（可圆角）
  C = 3,    -- column（垂直排版子节点）
  R = 4,    -- row（水平排版子节点）
  O = 5,    -- object（嵌入任意 Node，比如一张 Card 进 UI）
  ROOT = 7,
  S = 8,    -- slider
  I = 9,    -- input text box
  padding = 0,
}
```

**8 种节点**串联成一棵树。每个 UI 都是这个枚举的嵌套 lua 表。

## 2 · UIBox 与 UIElement

| 类 | 文件 | 角色 |
|--|--|--|
| `UIBox` | `engine/ui.lua:2` | 树的 root + owner，从 `Moveable` 派生 |
| `UIElement` | `engine/ui.lua:417` | 树里每个节点（含 ROOT/T/B/C/R/O/...），也从 Moveable 派生 |

`UIBox:init` 流程（行 18-99）做了 6 件事，都是布局 pass：

1. `set_parent_child(definition, nil)` — 把表结构 → UIElement 树
2. `calculate_xywh(UIRoot, T)` — 递归测量每节点 `T.{x,y,w,h}`
3. 把 root 的 w/h 写回 `self.T`
4. `UIRoot:set_wh()` — 容器宽高最终化
5. `UIRoot:set_alignments()` — 根据 `align="cm"`/`"tl"`/... 归位
6. `align_to_major()` + `initialize_VT(true)` — 接入 Moveable 的 VT 系统

**整个 UIBox 自身就是一个 Moveable**：alignment / role / juice / 阴影
全套继承自 01 笔记的 Moveable，所以 UI 也能 ease-in、抖动、阴影偏移。

## 3 · 一个 definition 示例（`cardarea.lua:378-431`）

```lua
self.children.area_uibox = UIBox({
  definition = {
    n = G.UIT.ROOT,
    config = { align = "cm", colour = G.C.CLEAR },
    nodes = {
      {
        n = G.UIT.R,
        config = {
          minw = self.T.w, minh = self.T.h,
          align = "cm", padding = 0.1, r = 0.1,
          colour = { 0, 0, 0, 0.1 },
          ref_table = self,                    -- 反向绑数据
        },
        nodes = {
          { n = G.UIT.T,
            config = { text = "DEFEAT", scale = 0.6,
                       colour = G.C.WHITE } },
          -- ...更多文本/列/行
        },
      },
    },
  },
  config = { align = "cm", offset = {x=0, y=0},
             major = self, parent = self },
})
```

**整个 UI = 嵌套 lua 表**。`UI_definitions.lua` 里几十个 `G.UIDEF.*` 函数
返回这种表。一棵中等复杂度的 UI（如 shop）能展开成几百行嵌套表。

## 4 · 反向数据流（`engine/ui.lua:141-146, 488-490`）

```lua
if node.config.ref_table and node.config.ref_value then
  node.config.text = tostring(node.config.ref_table[node.config.ref_value])
  if node.config.func and not recalculate then
    G.FUNCS[node.config.func](node)
  end
end
```

`ref_table[ref_value]` 模式让 UI 文本**自动同步状态**：lua 端每帧改一下
`G.GAME.dollars`，UI 上「$5」就跟着变；不需要手动通知 UI 重绘。还可以挂
`func = "some_callback"` 做条件显隐。这是 03 笔记 `_send` shader uniform
的同源设计——**lua 端用表反射做 binding**。

C++ 端没法直接抄这个：要么用宏 + offsetof 做 reflection（重），要么改成
**每帧 immediate-mode 直接读字段画文本**（轻、自然）。

## 5 · 为什么不移植

按移植成本 vs 收益拆：

### 5.1 移植代价

| 模块 | lua 行数 | C++ 端要重写量 |
|--|--|--|
| `ui.lua` layout 引擎（calculate_xywh / set_wh / set_alignments） | ~1000 | ~1500 行 + 字体度量适配 |
| `UIElement` 八种节点类型行为分发 | ~400 | 同等量 + raylib 字体接入 |
| `UI_definitions.lua` 所有 UI 表 | 16918 | **同量级**——Balatro 几乎所有 menu/HUD/popup 都在这里 |
| `ref_table/ref_value/func` 数据绑定 | 散落各处 | 要么宏黑魔法，要么改 immediate-mode 写法 |

总计**估 18000 行以上**，且不能跳——drop 一部分会缺 menu / shop / 设置。

### 5.2 收益（移植后能得到的）

- 一套**保留态布局引擎**（每帧不需要重计算盒模型）
- UI 自带 ease-in / 阴影 / juice
- 复杂嵌套 UI（settings menu）写起来像 React JSX

但这些 MVP 都不需要：

- HUD 只要 5 个数字（chips / mult / hands / discards / score / target）
- 卡牌 hover popup 是临时框，**immediate-mode 画一次就消失**
- shop / blind select 之类复杂 modal 用 raygui 或 ImGui 都能搞定，比抄 16k
  行 lua 表轻得多

### 5.3 raylib 端实测对比

raylib 自带 `DrawTextEx` / `DrawRectangleRounded` / `MeasureTextEx`，配合
raygui `GuiButton / GuiSlider`，**MVP 的全部 UI 估计 <500 行 C++**。

声明式 retained-mode 在 web 前端流行，但这是**单线程游戏循环**——immediate
mode 反而更直接：每帧 `if (hover) DrawTextEx(...)` 比维护一棵 UI 树简单
一个数量级。

## 6 · 那 Card 上挂的 UI 子节点怎么办？

02 笔记里 `Card.children` 包含若干 UIBox：

| 子节点 | 来源 | C++ 端替代 |
|--|--|--|
| `use_button` | hover Joker / consumeable 时弹的「Use / Sell」按钮 | hover 时 immediate-mode 画两个矩形按钮 + DrawTextEx；点击查 hit-test |
| `alert` | Joker 未发现 / 解锁的红点 | hover 检测 + `DrawCircle` 在卡角，5 行 |
| `focused_ui` | 手柄聚焦时的高亮框 | 手柄支持 Phase 8+，先 defer |
| `h_popup` | hover 详情卡（"Polychrome × 1.5"等） | hover 时画一块 DrawRectangleRounded + DrawTextEx |

`Card:highlight()`（`card.lua:6428`）里目前会 `UIBox(use_button definition)`
attach 到 Card —— C++ 端只需保留 `bool highlighted` 字段，画的事让
`Game::Draw()` 在遍历 hand 时统一 immediate-mode 画。

## 7 · 看哪些场景需要 UI

按现有 `G.UIDEF.*` 函数大致能数出 30+ UI 场景，MVP 影响度排：

| 场景 | MVP 必要 | C++ 端方案 |
|--|--|--|
| HUD（chips / mult / hands / discards / score / target / dollars / ante / round） | **必要** | `DrawTextEx` 直接画，~30 行 |
| Card hover popup（牌面详情）| 必要 | hover 检测 + immediate-mode 框 |
| Use / Sell / Skip 按钮 | 必要 | `DrawRectangleRounded` + 文本 + hit-test，~50 行/按钮 |
| Blind select / boss intro | Phase 6 | raygui modal |
| Shop | Phase 6 | raygui + 自绘卡牌 |
| Run info / Settings / Pause | Phase 8+ | raygui menu |
| Achievement / Unlock notification | Phase 9+ | defer |

## Port checklist

| Lua | 移植 | C++ 端 |
|--|--|--|
| `UIBox` 类 | **drop** | 不存在，HUD/popup 直接 immediate-mode |
| `UIElement` 类 | **drop** | 同 |
| `G.UIT.{T,B,C,R,O,ROOT,S,I}` 节点枚举 | **drop** | 不需要枚举，每个 UI 场景手写 |
| `set_parent_child` / `calculate_xywh` / `set_wh` / `set_alignments` layout 引擎 | **drop** | 用 raylib `MeasureTextEx` + 手算坐标 |
| `UI_definitions.lua` 所有 `G.UIDEF.*` 表 | **drop** | 每个 UI 场景一个 C++ 函数（raygui 或自绘） |
| `ref_table` / `ref_value` 数据绑定 | drop | immediate-mode 每帧直接读字段 |
| `G.FUNCS.*` 回调表 | drop | 直接函数指针或 lambda |
| `tooltip` / `detailed_tooltip` 系统 | rewrite | hover 检测 + immediate-mode 框，**简化版**：只支持 1 行 popup，不抄 retained-mode 的延迟 / 渐显逻辑 |
| Card 的 `use_button` / `sell_button` UIBox | rewrite | hover 时 immediate-mode 画两个 `DrawRectangleRounded` + `DrawTextEx` |
| Card 的 `alert` 红点 UIBox | rewrite | `DrawCircle` 在卡角，~5 行 |
| Card 的 `h_popup`（hover 详情） | rewrite | hover 检测 + popup box，~30 行 |
| Card 的 `focused_ui`（手柄聚焦） | defer | Phase 8+ 加手柄支持时再做 |
| HUD（chips / mult / hands / discards） | rewrite | `DrawTextEx` + 简单 layout，~30 行 |
| Blind select / Shop / Settings 等复杂 menu | defer | Phase 6+ 用 raygui 或 ImGui |
| `juice` 在 UI 节点上的传播（`set_values:496-509`） | drop | 不接入；HUD 数字弹一下用独立 tween |
| UI 上的 ease-in / 阴影 | drop | immediate-mode 没动画需求；要弹的字单独写 tween |
| 字体度量（`getWidth/getHeight + FONTSCALE + squish`） | rewrite | raylib `MeasureTextEx`；不抄 squish 字段 |
