---
source: ref-balatro/{cardarea,card}.lua
---

# 02 · CardArea / Card

`CardArea` 是装卡的容器（hand / play / deck / discard / joker / consumeable
/ shop / voucher / ...），`Card` 是单张牌的视觉实体。两者都从 `Moveable`
派生，所以 `T / VT / velocity / role / juice / pinch` 等字段在 01 笔记里讲过，
本篇只列各自新增的。

`CardArea` init 里关掉了 `drag/hover/click` —— 它本身**不响应输入**，输入
只走它装的 `Card`。

## 1 · CardArea

### 字段（`cardarea.lua:5-29`）

```lua
self.cards = {}                              -- 持有的 Card，左→右
self.highlighted = {}                        -- 选中子集（出牌待选）
self.config.type = config.type or "deck"     -- 决定 align_cards 走哪段
self.config.card_limit = config.card_limit or 52
self.config.temp_limit = self.config.card_limit  -- 布局留位宽度
self.config.highlighted_limit = config.highlight_limit or 5
self.card_w = config.card_w or G.CARD_W      -- 单卡宽度（≠ T.w）
self.shuffle_amt = 0                         -- deck 摇晃量
self.config.sort = config.sort or "desc"
table.insert(G.I.CARDAREA, self)             -- 全局登记
```

`config.type` 取值：`deck / discard / hand / play / shop / joker /
consumeable / voucher / title / title_2`，**每种都对应 `align_cards()` 里
一段独立公式**（§2 全部列出）。

### 关键方法

| 方法 | 行 | 作用 |
|--|--|--|
| `emplace(card, location, stay_flipped)` | 31 | 加卡：`location=="front"` 或 `type=="deck"` 首插，否则尾插；自动翻面 |
| `remove_card(card, discarded_only)` | 71 | 移除指定卡（或弹首/尾，由 type 决定）；从 `highlighted` 摘掉 |
| `draw_card_from(area, ...)` | 939 | `area:remove_card()` + `self:emplace()` |
| `add_to_highlighted` / `remove_from_highlighted` / `unhighlight_all` | 154 / 229 / 245 | 维护选中集合；shop / joker / consumeable 各有容量替换策略 |
| `parse_highlighted()` | 191 | 仅 `G.hand` 有意义：评估当前选中能组成什么牌型，刷新 HUD |
| `set_ranks()` | 258 | 写入 `card.rank`；按 type 决定哪些卡可拖 |
| `align_cards()` | 615 | **核心**：按 `config.type` 重算每张卡的 `T.x/y/r` |
| `hard_set_T / hard_set_cards` | 891 / 902 | 跳过 ease，瞬时把 VT 设到 T |

注意 `emplace` 的"插入位置"：deck 永远首插（视觉上从堆顶抽走），其它默认
尾插。

## 2 · 排列公式

每帧在 `Moveable:move` 之后调 `align_cards`，**直接覆写 `card.T.x/y/r`**，
随后由 Moveable 的 ease 自然产生平滑插值（01 笔记 §3）。

### 共用记号

下面把这两个简写抽出来，公式里直接展开：

- `n = #self.cards`，`M = max(n, temp_limit)`
- `lerp_x(k) = (k-1)/max(M-1,1) - 0.5*(n-M)/max(M-1,1)`
  把第 k 张映到 0..1，且 `n<M` 时整体居中
- `slot_x(k) = self.T.x + (self.T.w - card_w) * lerp_x(k)
              + 0.5*(card_w - card.T.w)`

每段末尾还会统一加 `card.T.x += card.shadow_parrallax.x / 30`（视差），
公式里略掉。

### 2.1 Hand 弧形（`cardarea.lua:692-722`）

```lua
card.T.r = 0.2 * (-#self.cards/2 - 0.5 + k) / #self.cards
  + (G.SETTINGS.reduced_motion and 0 or 1)
    * 0.02 * math.sin(2 * G.TIMERS.REAL + card.T.x)
local max_cards = math.max(#self.cards, self.config.temp_limit)
card.T.x = self.T.x
  + (self.T.w - self.card_w) * (
      (k-1) / math.max(max_cards-1, 1)
      - 0.5 * (#self.cards - max_cards) / math.max(max_cards-1, 1)
    )
  + 0.5 * (self.card_w - card.T.w)
card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height
  + (G.SETTINGS.reduced_motion and 0 or 1)
    * 0.03 * math.sin(0.666 * G.TIMERS.REAL + card.T.x)
  + math.abs(0.5 * (-#self.cards/2 + k - 0.5) / #self.cards)
  - 0.2
```

逐项常量（**直接抄到 C++**）：

| 项 | 数值 | 含义 |
|--|--|--|
| `0.2 * (...) / n` | 端点 ±0.1 rad | 扇形旋转 |
| `0.02 * sin(2t + x)` | ±0.02 rad | r 抖动；相位用 `card.T.x` 解耦每张卡 |
| `0.03 * sin(0.666 t + x)` | ±0.03 单位 | y 抖动；频率约 r 抖动的 1/3 |
| `abs(0.5*(...)/n)` | 0..0.25 | **弧形高度**：两端大、中间 0 |
| `-0.2` | 整体下偏 | 把"中间高"补成"两端高、中间略低于上沿" |
| `highlight_height` | `G.HIGHLIGHT_H` | 选中时拔高，否则 0 |

末尾按视觉 x 排序 `self.cards`（行 719-721），drag 时跨过相邻卡能立即换位。

### 2.2 Play / shop 直线（`cardarea.lua:787-810`）

```lua
card.T.r = 0
local max_cards = math.max(#self.cards, self.config.temp_limit)
card.T.x = self.T.x
  + (self.T.w - self.card_w) * (
      (k-1) / math.max(max_cards-1, 1)
      - 0.5 * (#self.cards - max_cards) / math.max(max_cards-1, 1)
    )
  + 0.5 * (self.card_w - card.T.w)
  + (self.config.card_limit == 1 and 0.5 * (self.T.w - card.T.w) or 0)
card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height
```

无旋转无抖动，y 固定居中。`card_limit == 1` 时单卡撑满整宽（出过牌后单卡
居中）。`shop` 走同一段。

### 2.3 Joker / consumeable / title_2（`cardarea.lua:811-852`）

```lua
card.T.r = 0.1 * (-#self.cards/2 - 0.5 + k) / #self.cards   -- hand 一半
  + 0.02 * math.sin(2 * G.TIMERS.REAL + card.T.x)
-- 三档 x 分支
if #cards > 2
   or (#cards > 1 and self == G.consumeables)
   or (#cards > 1 and self.config.spread) then
  card.T.x = self.T.x + (self.T.w - card_w) * ((k-1)/(#cards-1))
           + 0.5 * (card_w - card.T.w)
elseif #cards > 1 then
  card.T.x = self.T.x + (self.T.w - card_w) * ((k-0.5)/#cards)
           + 0.5 * (card_w - card.T.w)
else
  card.T.x = self.T.x + self.T.w/2 - card_w/2
           + 0.5 * (card_w - card.T.w)
end
card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height/2
  + 0.03 * math.sin(0.666 * G.TIMERS.REAL + card.T.x)
```

要点：

- joker 槽位**真等距** `(k-1)/(n-1)`，不像 hand 用 `temp_limit` 留空。
- 2 张时走 `(k-0.5)/n` 让两张居中，避免端点贴边。
- `consumeable` 永远走"等距"分支（即使 2 张也撑开两端）。
- `highlight_height / 2`：joker 选中拔得更克制。
- 排序 key `T.x - 100*(pinned and sort_id or 0)`：pinned joker 强行排到最左。

### 2.4 其他 type（一句话定位）

| type | 行 | 简述 |
|--|--|--|
| `deck` | 624-643 | 整副堆叠 + `shuffle_amt` 摇晃；y 用 `shadow_parrallax.y * deck_height` 制造厚度 |
| `discard` | 644-656 | 用 `card.discard_pos`（init 时一次 random）零散撒 |
| `hand` 开包态 | 657-688 | 弧度 `0.4`（更夸张），y 项 `^2` 形成更深弧 |
| `title` / 单 voucher | 723-752 | 同 hand 公式，开场动画用 |
| 多 voucher | 753-786 | 在 hand 公式上加 `±0.27` 横偏移 + `±0.08` rad 让相邻互错 |
| `consumeable` 单段 | 853-882 | 等距 + y 抖动频率 `2*1.666` |

## 3 · Card

### 字段（`card.lua:5-87`）

```lua
self.CT = self.VT                          -- 命中检测用 VT，拖拽中也精准
self.config = { card = card, center = center }
self.tilt_var = { mx, my, dx, dy, amt = 0 }    -- hover 3D 视差
self.ambient_tilt = 0.2
self.states.collide.can = true             -- Card 启用全部交互
self.states.hover.can = true
self.states.drag.can = true
self.states.click.can = true
self.children = {}
self.children.shadow = Moveable(0, 0, 0, 0)    -- 占位，draw 直接走 center/back
-- set_ability / set_base 之后填上：
-- children.{front, back, center}: Sprite，
--   role_type="Glued"，draw_major=self
self.facing = "front"; self.sprite_facing = "front"
self.flipping = nil                        -- "f2b" / "b2f" / nil
self.area = nil                            -- 反向回指 CardArea
self.highlighted = false
self.T.scale = 0.95
self.discard_pos = { r, x, y }             -- init 时一次 random
self.unique_val = 1 - self.ID / 1603301    -- 每卡稳定噪声种子
self.playing_card = self.params.playing_card
table.insert(G.I.CARD, self)
```

| 字段 | 用途 |
|--|--|
| `CT = VT` | 命中检测用**视觉 transform**——拖拽中的卡仍然能精准点击 |
| `config.center` | Joker / Tarot / 牌背 / enhancement 的中心定义；`set_ability` 写它 |
| `config.card` | 标准扑克的 rank+suit 数据；`set_base` 写它 |
| `children.front` | rank+suit 合成图（playing card 才有） |
| `children.center` | enhancement / Joker / Tarot / Voucher 等"特殊层" |
| `children.back` | 牌背图，按 `sprite_facing` 与 `front` 互斥渲染 |
| `children.floating_sprite` | Joker soul 层（仅有 `soul_pos` 的 Joker） |
| `children.use_button / .alert / .focused_ui` | UIBox 子节点，按需 attach |
| `tilt_var` / `ambient_tilt` | hover 视差量，最终走到 shader uniform |
| `unique_val` | 每张卡的稳定噪声种子（fract、whirl 等 shader 用） |

子 Sprite 的 `set_role({role_type="Glued", draw_major=self})` 表示其 T/VT
完全锁到 Card —— 子节点不参与 ease，跟着 Card 的 VT 整体移动（01 笔记
§3 「Glued」）。

### 翻牌动画（`card.lua:5776` + `card.lua:5789`）

```lua
function Card:flip()
  if self.facing == "front" then
    self.flipping = "f2b"; self.facing = "back"
    self.pinch.x = true                    -- Moveable.move_wh 把 VT.w 收到 0
  elseif self.facing == "back" then
    self.flipping = "b2f"; self.facing = "front"
    self.pinch.x = true
  end
end

function Card:update(dt)
  if self.flipping == "f2b" and self.VT.w <= 0 then
    self.sprite_facing = "back"; self.pinch.x = false
  end
  if self.flipping == "b2f" and self.VT.w <= 0 then
    self.sprite_facing = "front"; self.pinch.x = false
  end
end
```

机制：`pinch.x = true` 让 `Moveable.move_wh` 把 `VT.w` 收到 0 → 当帧
`VT.w <= 0` 切换 `sprite_facing` → `pinch.x = false`，`VT.w` 自然 ease
回 `T.w`。**视觉上是"压扁→中线切图→恢复"**——一维 pinch 完成 flip，
不需要 3D matrix。

这也解释了为什么 Card 必须从 Moveable 派生：flip 借用了 `pinch + VT/T 双
transform` 这套机制。

### 分层 draw（`card.lua:6027`）

```lua
function Card:draw(layer)
  layer = layer or "both"
  if layer == "shadow" or layer == "both" then ... end
  G.shared_shadow = (sprite_facing == "front")
                    and children.center or children.back
  -- 然后依次画 children.{back / front / center / floating_sprite}
end
```

`Game:draw()` **两遍**遍历所有 Card：第一遍 `layer="shadow"` 把所有阴影
画到底层，第二遍 `layer="card"` 画卡面 —— 避免阴影互相遮挡（详见 06 笔记）。

shader uniform `send_to_shader[1] = VT.r*3 + TIMERS.REAL/28
+ juice.r*20 + tilt_var.amt` 在这里准备，但 `setShader` 上传发生在子
Sprite 的 draw（03 笔记）。

## Port checklist

| Lua | 移植 | C++ 端 |
|--|--|--|
| `CardArea.cards / highlighted` | keep | `std::vector<Card*>` ×2 |
| `CardArea.config.type` 字符串分支 | rewrite | `enum class AreaType { Hand, Play, Joker, Consumeable, Shop, Deck, Discard }` + `switch` |
| `config.{card_limit, temp_limit, card_w, lr_padding, sort}` | keep | 同字段直译 |
| `CardArea:emplace / remove_card / draw_card_from` | keep | 接口直译；deck 首插 vs hand 尾插差异保留 |
| `CardArea:add_to_highlighted` 三档（shop / joker / hand） | keep | 用 AreaType switch 区分 |
| `CardArea:parse_highlighted` | defer | Phase 6 实现，依赖 `get_poker_hand_info` 整个评估器 |
| `CardArea:set_ranks` | keep | 同 |
| `CardArea:align_cards` 派遣 | **keep** | Phase 5 必做；先实现 hand+play+joker 三档够 MVP |
| **hand 弧形公式** `cardarea.lua:692-722` | **keep** | 常量 `0.2 / 0.02 / 0.03 / 0.5 / 0.666 / 0.2` 全保留；`reduced_motion` 留 bool |
| **play 直线公式** `cardarea.lua:787-810` | **keep** | `card_limit==1` 单卡居中分支保留 |
| **joker / consumeable 公式** `cardarea.lua:811-882` | keep | 三档 x 分支照抄；`highlight_height/2` 保留 |
| deck / discard / voucher / title 公式 | defer | Phase 5 之后；deck 视差公式 Phase 7 再补 |
| `pinned` 排序（joker） | defer | sticker 系统出来再做 |
| `Card.children.{shadow, front, back, center}` 四层 | keep | 4 个 `Sprite` 成员，"Glued"-like 锁到 Card |
| `Card.children.floating_sprite`（Joker soul） | defer | Phase 7 |
| `Card.facing / sprite_facing / flipping + pinch.x` flip | **keep** | Phase 5 一并实现，复用 Moveable 的 pinch |
| `Card:flip / update` flip 状态机 | keep | 直译 |
| `Card.tilt_var / ambient_tilt` | keep | hover 视差，shader uniform 用 |
| `Card.discard_pos` | keep | init 时 `Random()` 一次 |
| `Card.unique_val = 1 - ID/1603301` | keep | shader 噪声种子，公式不变 |
| `Card.children.use_button / .alert / .focused_ui` | rewrite | UIBox 整体重写（05 笔记），Card 这边只留 attach 接口 |
| `Card:set_ability / set_base / set_sprites` 三入口 | rewrite | C++ 端单一 `Card::SetCenter(const CenterDef*)` 入口 |
| `Card:highlight()` 里 `use_button` 创建 | defer | UIBox ready 之前只翻 `highlighted` 标志 |
| `Card:draw(layer)` 两遍 shadow / card 分层 | keep | 同 06 笔记 |
| `G.I.CARDAREA / G.I.CARD` | rewrite | entt views（同 01 笔记） |
