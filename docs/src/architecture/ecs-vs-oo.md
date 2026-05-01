---
source: 架构选型——是否用 EnTT ECS
---

# ECS vs OO

> **结论先行**：MVP 阶段**直接 OO**，5 个核心类 + `std::vector<unique_ptr>`
> 维护全局集合替代 `G.I.*`。Phase 7+ 真出现批处理性能瓶颈或 Joker 修饰器
> 组合爆炸时再上 [EnTT](https://github.com/skypjack/entt)。

## 1 · Balatro 是怎么做的

01 笔记讲过 Balatro 的引擎抽象只有三层：

```text
Object        -- lua metatable 模拟的 OOP 基类
  └─ Node     -- 带 transform 的场景节点
      └─ Moveable  -- T/VT 双 transform + ease + juice
          └─ Sprite / CardArea / Card / Particles / UIBox
```

**纯 OO + 单继承**，没拆组件。每个 Movable 子类自己持有自己的字段。

`G.I.*` 只是**集合 query**，不是 ECS：

```text
G.I.NODE      -- 全部 Node
G.I.MOVEABLE  -- 全部 Movable
G.I.SPRITE    -- 全部 Sprite
G.I.CARD      -- 全部 Card
G.I.CARDAREA  -- 全部 CardArea
G.I.UIBOX     -- 全部 UIBox（不含 POPUP）
G.I.POPUP     -- 弹出 UIBox
```

`Game:draw()` 直接 `for v in G.I.X: draw(v)`——批处理走遍历，**没有
component view、archetype、storage**。能做到这点是因为 lua 表本来就是
不定结构，遍历开销可忽略；C++ 同等做法 `std::vector<Card*>` 顺序访问
也是 O(n)。

## 2 · ECS 的卖点 vs Balatro 的实际需求

| ECS 卖点 | 在我们项目中的实际收益 |
|--|--|
| 批处理 update / draw（archetype 内连续内存） | **MVP 全部对象 < 100 个**（手牌 8 + joker 5 + consumeable 2 + shop 6 + ...）。线性 vector 已经 cache 友好 |
| Composition over inheritance | Balatro 的继承链 ≤ 3 层，没继承爆炸问题 |
| Views 自动 query | 用 `vector<unique_ptr<Card>>` 替代 `G.I.CARD` 同样 O(1) 拿 view |
| 数据 / 行为分离 | Card 的字段 + 行为高度耦合（flip / hover / draw 都要看 facing/sprite_facing），拆开徒增样板 |

## 3 · ECS 在我们这种项目里的痛点

### 3.1 场景图

01 笔记里 Card 的 `children = {shadow, front, back, center}` 是树形子节点。
ECS 表达 hierarchy 要：

- 加 `Parent` / `Children` 组件
- 每帧维护层级 dirty flag
- view + sort 才能保证父先 update / draw

而 Balatro 直接 `for v in self.children: v:draw()` 一行解决。C++ OO 同样
一行。**这是 ECS 不擅长的领域**。

### 3.2 状态机

`Card.flipping ∈ {nil, "f2b", "b2f"}` + `facing` + `sprite_facing` + `pinch.x`
是个紧耦合 4 字段状态机（02 笔记 §5）。ECS 里一种做法是把每个状态搞成一
个 tag component（`FlippingF2B` / `FlippingB2F` / ...），切换时 add/remove，
**比一个 enum 字段写 5 倍代码量**。

Balatro 的 `Movable.role.role_type = "Major" | "Minor" | "Glued"` 也类似。

### 3.3 修饰器组合（Phase 7+ 的真实威胁）

Joker × Edition × Seal × Sticker × Stake 5 维互相组合，每个 Joker 又有
~150 种独立行为。**这种组合爆炸才是 ECS 真正的菜**：

- 每个修饰器一个 component
- view 自动找出"有 Foil 的 Card"或"有 Polychrome 且 highlighted 的 Card"
- 算分时按 component 类型分别 dispatch

但**只有到 Phase 7 我们才会碰**。MVP 阶段（Phase 3-6）只需要"画一张卡 +
弧形排列 + 出牌评估"，OO 完全够。

## 4 · 我们的决策

### 4.1 类层级

5 个核心类（直接对应 Balatro 三层 + Card / CardArea）：

| C++ 类 | 对应 lua | 角色 |
|--|--|--|
| `Transform` | `Node.T / Movable.T` | POD struct，6 字段 (x,y,w,h,r,scale) |
| `Movable`（基类） | `Moveable` | 持有 T / VT / velocity / juice / pinch；virtual `Update / Move / Draw` |
| `Sprite` : `Movable` | `Sprite` | atlas + Rectangle + 多 pass shader |
| `Card` : `Movable` | `Card` | 4 个 Sprite 子 + flip 状态机 |
| `CardArea` : `Movable` | `CardArea` | `vector<Card*>` + AreaType + align_cards |

**不要 Object / Node 这两层抽象**——01 笔记 port checklist 已经 drop 了。

### 4.2 全局集合

```text
class Game {
  std::vector<std::unique_ptr<Movable>>  movables;
  std::vector<Card*>                     cards;       // 非 owning
  std::vector<CardArea*>                 cardAreas;   // 非 owning
  std::vector<Sprite*>                   sprites;     // 非 owning
  // ...
};
```

`movables` 拥有所有 Movable（含 Card / CardArea / Sprite，多态）；
`cards / cardAreas / sprites` 是同一对象的非 owning 视图。**与 Balatro
`G.I.*` 一一对应**——遍历 / 加入 / 删除接口对齐 Balatro。

`Movable` 的析构在 destroy 时同步从 vector 里移除（弱引用清理）。

### 4.3 何时引入 EnTT

下面任一条件触发就上 EnTT：

1. 同时存在的 Movable 对象数 > 1000（粒子大量喷发会触发）
2. Joker 修饰器多到 OO 写不下（Phase 7 后期）
3. profile 显示线性遍历是热点（不太可能在 MVP 出现）

切换成本：把 5 个核心类改成 component，view 替代 vector 遍历——估计
3-5 天工作量。但**不要预先做**，YAGNI。

## 5 · 对应表（Balatro lua → 我们的 C++）

| Balatro | 我们 |
|--|--|
| `Object:extend` | `class : public Base` |
| `Foo()` 调 init | 构造函数 |
| `self.children` | `std::vector<Movable*>` |
| `G.I.CARD` 数组 | `Game::cards` 视图 |
| `getmetatable(self) == Card` 类型测 | `dynamic_cast` 或 enum tag |
| `G.E_MANAGER:add_event(Event{ ref_table, ref_value, ease_to })` | 自写 `Tween` 类，存 `void*` 字段地址 + `T target` |
| `G.FUNCS.foo(node)` 回调 | `std::function<void(Movable*)>` 或函数指针 |
| `pseudorandom / pseudoseed` | `std::mt19937` + 命名 seed |

## 6 · 不引入的设计

明确**不**做：

- **EnTT / flecs / 任何 ECS 库**（直到 Phase 7+ 触发条件）
- **Bevy 风格 system 函数**（lua 没有，C++ 引入会语义不一致）
- **响应式数据绑定**（替代 `ref_table/ref_value`）—— 见 05 笔记，
  immediate-mode 直接读字段更简单
- **观察者模式 / event bus**（Balatro 没有，加进来是过度工程）

> **标语**：能用 `vector + virtual` 解决的就别上 ECS。Phase 7 见。
