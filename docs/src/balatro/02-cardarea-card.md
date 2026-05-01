---
source: ref-balatro/{cardarea,card}.lua
---

# 02 · CardArea / Card

`CardArea` 是卡牌容器（手牌、出牌区、joker 区、牌堆等）。
`Card` 是单张牌。两者都继承自 `Movable`，所以"位置"全靠
T/VT 双 transform 自动平滑（见 [01](./01-object-node-moveable.md)）。

## 1 · CardArea

### 字段
```lua
self.cards            = {}              -- 有序数组
self.highlighted      = {}              -- 选中卡列表
self.config = {
  type            = "deck",             -- 见下表
  card_w          = G.CARD_W,
  card_limit      = 52,
  highlighted_limit = 5,
  lr_padding      = 0.1,
  sort            = "desc",
}
self.shuffle_amt = 0                    -- 洗牌时让 deck 抖动
```
所有 CardArea 也注册到 `G.I.CARDAREA`。

### `config.type` 枚举与排列策略

不同 type 用**完全不同的公式**重排卡。`align_cards()` 是一个大 if-else
（`cardarea.lua:615-889`）。下面是 MVP 关心的几种：

| type | 排列 | 关键公式（精简） | 行号 |
|--|--|--|--|
| `deck`        | 厚度堆叠 | x/y 各加 `shadow_parrallax * deck_height * (n-k)`；`shuffle_amt` 注入 r 抖动 | 624-643 |
| `discard`     | 随机散落 | 用每张卡 init 时随机生成的 `discard_pos.{x,y,r}` | 644-656 |
| `hand`        | **弧形** | `r = 0.2 * (-n/2 - 0.5 + k) / n + 0.02*sin(2t + x)`；y 抛物线；每帧按 x 重排 | 692-722 |
| `play` / `shop` | 直线 r=0 | x 均分；y 居中 | 787-810 |
| `joker` / `title_2` | 直线带 wiggle | x 均分但 `n>2` 与 `n<=2` 间距策略不同 | 811-852 |
| `consumeable` | 直线 | x 均分；只 `n>1` 时铺开 | 853-882 |

> 没有可插拔 layout 策略对象。**MVP 直接 if-else 抄过来即可**。

### Add / remove / draw_card_from

```lua
emplace(card, location, stay_flipped)
  -- location == "front" 或 type == "deck" 时插到 cards[1]
  -- 否则 append 到末尾
  -- 然后 set_card_area(self) → set_ranks() → align_cards()

remove_card(card, discarded_only)  -- 从 cards 数组移除
draw_card_from(area, ...)           -- 从另一个 area 抽一张过来
```

`set_ranks()` 重新打 `card.rank = k`，并按 type 锁定/解锁 drag/collide
状态（deck 顶以下不能拖、play/shop/consumeable 不可拖等）。

### Highlight 流程

```lua
add_to_highlighted(card)        -- 加进 highlighted[] + card:highlight(true)
remove_from_highlighted(card)
unhighlight_all()
parse_highlighted()             -- hand 区改变 highlight 时重算扑克手牌
```

`parse_highlighted()` 调 `G.FUNCS.get_poker_hand_info(highlighted)` 得出
hand name + chips + mult + level，写进 HUD 文字（`update_hand_text`）。
**MVP 必须移植这条链**，是 select-and-play 的核心。

### Draw 顺序（精简自 `cardarea.lua:325-510`）

1. 跳过透明 type（discard/voucher/play/consumeable/title 等不画 card area
   的"框"）
2. 画 `area_uibox`（卡数显示 `card_count / card_limit`）
3. `draw_layers = {"shadow", "card"}`，每层遍历卡：
   - 跳过 dragging / focused 的卡（这些最后由 game 单独画在最上层）
   - deck 区做厚度优化：只画 1 / 末尾 / 每 `thin_draw=9` 张
   - hand/play/voucher/title：按数组顺序画
   - joker/consumeable/shop：先画 `!highlighted`，再画 `highlighted`，
     使高亮卡盖在普通卡之上

## 2 · Card

### 字段（核心）

```lua
self.config       = { card, center }    -- card = rank/suit; center = joker等
self.ability      = {}                  -- 运行时数据（mult、chips、…）
self.facing       = "front"             -- "front" | "back"
self.sprite_facing = "front"            -- 翻牌动画用
self.flipping     = nil                 -- "f2b" | "b2f" | nil
self.highlighted  = false
self.debuff       = false
self.area         = nil                 -- 当前所属 CardArea
self.rank         = nil                 -- 在 area.cards 里的下标
self.sort_id      = G.sort_id            -- 全局递增稳定排序键
self.click_timeout = 0.3
self.discard_pos  = { x, y, r }         -- init 时随机，给 discard area 用
self.tilt_var     = { mx, my, dx, dy, amt }  -- 鼠标 hover 倾斜
self.ambient_tilt = 0.2
self.edition      = nil                 -- foil/holo/polychrome/negative
self.children = { shadow, front, back, center }  -- 多 sprite 叠加
self.T.scale      = 0.95                -- 默认缩 0.95，hover 时 zoom
```

**一张卡在视觉上是 4 个 Sprite 子对象的堆叠**（`shadow / front / back / center`），
edition / seal / sticker 是在 `front` 的 `Sprite.draw_steps` 里
追加的多 shader pass —— 见 [03](./03-sprite-shader.md)。

### 翻牌动画 = pinch + VT 自动收缩

```lua
function Card:flip()
  if self.facing == "front" then
    self.flipping = "f2b"; self.facing = "back"
    self.pinch.x = true                 -- VT.w 自动 ease 到 0
  elseif self.facing == "back" then
    self.flipping = "b2f"; self.facing = "front"
    self.pinch.x = true
  end
end
```
`update(dt)` 检测 `VT.w <= 0` 时切换 `sprite_facing`，关掉 `pinch.x`
让宽度 ease 回 `T.w`。**整个翻牌动画零状态机**，全靠 Movable.pinch +
VT 自然过渡。

### 选中 / 点击 / 释放

```lua
function Card:click()
  if self.area and self.area:can_highlight(self) then
    if not self.highlighted then self.area:add_to_highlighted(self)
    else                          self.area:remove_from_highlighted(self) end
  end
end

function Card:highlight(on)
  self.highlighted = on
  -- joker/consumeable/pack 卡高亮时挂上 use/sell 按钮 UIBox
  if on and (self.ability.set == "Joker" or self.ability.consumeable) then
    self.children.use_button = UIBox(...)
  end
end

function Card:release(dragged)
  if dragged:is(Card) then self.area:release(dragged) end
end
```

select 状态在 `area.highlighted[]` 数组，**不在卡上**——卡只持
`highlighted` bool。这意味着多选 / 限制由 area 管。

### Hover 倾斜（3D 假效果）

`hover()` 持续更新 `tilt_var.mx/my`（鼠标相对卡中心位置），
shader 用这个产生伪 3D 倾斜。`ambient_tilt = 0.2` 是无 hover 时
的基础倾斜。详见 [03](./03-sprite-shader.md)。

### `set_card_area` / `remove_from_area`

```lua
function Card:set_card_area(area)
  self.area = area; self.parent = area
  self.layered_parallax = area.layered_parallax
end
function Card:remove_from_area()
  self.area = nil; self.parent = nil
  self.layered_parallax = { x = 0, y = 0 }
end
```
**只改引用，不调动画**。卡新位置由 `area:align_cards()` 设的 `T`
驱动 VT 自动追上。

## Port checklist

| Lua 字段/方法 | 移植 | C++ 端 |
|--|--|--|
| `CardArea.cards / highlighted` | keep | `std::vector<Card*>` / `std::vector<Card*>` |
| `CardArea.config.type` 枚举 | keep | `enum class AreaType { Deck, Hand, Play, Joker, Consumable, Discard, Shop }` |
| `CardArea.config.{card_limit, highlighted_limit, card_w, lr_padding, sort}` | keep | struct |
| `CardArea:align_cards()` | **keep** | 一份 switch by AreaType，**hand/play/joker 公式直接抄** |
| `CardArea:emplace / remove_card / draw_card_from` | keep | 同 API |
| `CardArea:add_to_highlighted / remove_from_highlighted / unhighlight_all` | keep | 同 |
| `CardArea:parse_highlighted` (扑克手牌识别) | **keep** | Phase 6 实现 |
| `CardArea:set_ranks` | keep | 重排时打 `card.rank` 同时锁 drag/collide |
| `CardArea:draw` 多 layer + 跳过 dragging | keep | 同顺序，dragging 卡由 GameLayer 最后画 |
| `area_uibox`（card_count/limit 显示） | rewrite | 用 raygui DrawTextEx 直接画 |
| `peek_deck` / `view_deck` UIBox | defer | MVP 不需要 |
| `Card.config.{card, center}` | keep | `struct CardData{ Rank rank; Suit suit; }` + `CenterData*` |
| `Card.ability` (运行时 mult/chips) | keep | `struct Ability` |
| `Card.facing / sprite_facing / flipping` | keep | 同字段 |
| `Card.highlighted / debuff / area / rank / sort_id` | keep | 同 |
| `Card.discard_pos` | keep | init 时随机 |
| `Card.tilt_var / ambient_tilt` | keep | hover shader 用 |
| `Card.edition / seal / sticker` | keep | 但绘制走 Sprite.draw_steps |
| `Card.children = {shadow, front, back, center}` | keep | 4 个 Sprite 子对象 |
| `Card.T.scale = 0.95` 默认 | keep | 同 |
| `Card:flip` (pinch.x 翻牌) | **keep**（精彩） | 直接抄，依赖 Movable.pinch |
| `Card:click / highlight / release` | keep | virtual override + 委托 area |
| `Card:set_card_area / remove_from_area` | keep | 只改 area 指针 |
| `Card:juice_up` | keep | 已在 Movable |
| `use_button` / `sell_button` UIBox | defer/rewrite | Phase 6 用 raygui Button 重做 |
| `Card:save / load` | defer | 没 save 系统前不做 |
| `Card:explode / shatter / dissolve / materialize` | defer | shader 特效，Phase 7 |
