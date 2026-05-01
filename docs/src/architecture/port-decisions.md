---
source: 汇总自 balatro/01-06.md 的 port checklist
---

# Port Decisions（汇总表）

把 `balatro/01-06.md` 末尾每篇的 port checklist 抽出来，按**系统**重组、
按**Phase**排序。Phase 2+ 实施时这是单一参考——不必再翻 6 篇笔记。

四档决策：

- **keep**：lua 端公式、字段、流程**逐字抄**到 C++（包括魔法常数）
- **rewrite**：保留语义但实现改写（C++ 惯用法替代 lua 反射等）
- **drop**：完全不移植
- **defer**：MVP 不做，先占位

## 1 · 引擎核心（Phase 3）

`balatro/01-object-node-moveable.md`

| Lua | 决策 | 备注 |
|--|--|--|
| `Object:extend / __call(init) / is / super` | drop | C++ 原生 class + virtual + base class access |
| `Node.T = {x,y,w,h,r,scale}` / `CT` | keep | 一个 `Transform` struct |
| `Node.click_offset / hover_offset` | keep | 同 |
| `Node.container` | rewrite | 父指针（只一层 `G.ROOM`，不必通用） |
| `Node.children` | rewrite | `std::vector<Movable*>` 或 entt hierarchy |
| `Node.states.*`（visible/hover/click/drag/collide/focus/release_on） | keep | bitmask 或 struct 直译 |
| `Node.ID` | keep | 自增 uint64 或 `entt::entity` |
| `Node:collides_with_point` | keep | AABB + 反向 container transform |
| `Node:hover/drag/click/release/animate/update` | keep | virtual 钩子 |
| `Node:draw_boundingrect` | drop | debug 专用，raylib `DrawRectangleLines` 替代 |
| `Movable.T / VT / velocity` | **keep** | 核心字段 |
| `Movable.role.*`（Major/Minor/Glued, bond） | defer | MVP 不做；Phase 7 再看 |
| `Movable.alignment` 字符代码 (`cm`/`tli`) | defer | 用枚举 + offset 重写，不抄字符代码 |
| `Movable.juice` / `move_juice` | keep | sin 衰减公式直接抄 |
| `Movable.pinch` | keep | flip 用 |
| `Movable.shadow_parrallax / shadow_height` | keep | 同 |
| `Movable:move(dt)` exp ease 主循环 | **keep** | 公式抄 `moveable.lua:453-480` |
| `G.exp_times.{xy=50, scale=60, r=190, max_vel}` | keep | 一组 const float |
| `Movable:hard_set_T / hard_set_VT` | keep | 同 |
| `G.I.*` 全局集合 | rewrite | entt views，不再人工维护数组 |
| `G.STAGE_OBJECTS[stage]` | defer | 没切场景需求前不做 |

## 2 · CardArea / Card（Phase 5）

`balatro/02-cardarea-card.md`

| Lua | 决策 | 备注 |
|--|--|--|
| `CardArea.cards / highlighted` | keep | `std::vector<Card*>` ×2 |
| `CardArea.config.type` 字符串分支 | rewrite | `enum AreaType { Hand, Play, Joker, Consumeable, Shop, Deck, Discard }` |
| `config.{card_limit, temp_limit, card_w, lr_padding, sort}` | keep | 同字段直译 |
| `emplace / remove_card / draw_card_from` | keep | 接口直译；deck 首插 vs hand 尾插差异保留 |
| `add_to_highlighted` 三档（shop/joker/hand） | keep | AreaType switch |
| `parse_highlighted` | defer | Phase 6 再做（依赖 poker hand 评估器） |
| `set_ranks` | keep | 同 |
| **hand 弧形公式**（`cardarea.lua:692-722`） | **keep** | 常量 0.2 / 0.02 / 0.03 / 0.5 / 0.666 / -0.2 全保留 |
| **play 直线公式**（`cardarea.lua:787-810`） | **keep** | `card_limit==1` 单卡居中分支保留 |
| **joker / consumeable 公式**（`cardarea.lua:811-882`） | keep | 三档 x 分支照抄；`highlight_height/2` |
| deck / discard / voucher / title 公式 | defer | Phase 5 之后；deck 视差 Phase 7 |
| `pinned` 排序 | defer | sticker 系统出来再做 |
| `Card.children.{shadow, front, back, center}` 四层 | keep | 4 个 Sprite 成员，"Glued" 锁到 Card |
| `Card.children.floating_sprite` | defer | Phase 7 |
| `Card.facing / sprite_facing / flipping + pinch.x` flip | **keep** | 复用 Movable 的 pinch |
| `Card.tilt_var / ambient_tilt` | keep | hover 视差，shader uniform 用 |
| `Card.discard_pos` | keep | init 时 random 一次 |
| `Card.unique_val = 1 - ID/1603301` | keep | shader 噪声种子 |
| `Card.children.{use_button, alert, focused_ui, h_popup}` | rewrite | UIBox 整体 drop，改 immediate-mode（见 §5） |
| `Card:set_ability / set_base / set_sprites` 三入口 | rewrite | C++ 端单一 `SetCenter(const CenterDef*)` |
| `Card:draw(layer)` 内部 shadow / card 分段 | keep | 单函数内两段 if，不外层循环 |

## 3 · Sprite / Shader（Phase 4 / 2）

`balatro/03-sprite-shader.md`

| Lua | 决策 | 备注 |
|--|--|--|
| `Sprite.atlas / scale / sprite_pos / sprite (Quad)` | keep | `Texture2D` + `Rectangle` 切片 |
| `set_sprite_pos` quad 重建 | keep | 改 Rectangle 即可 |
| `sprite_pos.v` 随机帧 | defer | 动画 atlas 实现时再做 |
| `Sprite.image_dims / scale_mag` | keep | shader uniform 用 |
| `Sprite:draw_self` | keep | `DrawTexturePro` |
| `prep_draw` push/scale/translate/rotate 链 | keep | 直接搬到 helper；常量 `TILESCALE * TILESIZE` |
| `Sprite:draw_from`（借 transform） | defer | MVP 不需要 |
| `Sprite.draw_steps` 多 pass | keep | `vector<DrawStep>`，按顺序 BeginShader → Draw → EndShader |
| `Sprite:draw_shader` 共享 uniform 上传 | **keep** | 一个 `UploadStdUniforms` 函数 |
| `time = 123.33412 * (ID/1.14212) % 3000` | **keep** | 公式不变 |
| `texture_details = (sprite_pos.x, sprite_pos.y, atlas.px, atlas.py)` | keep | vec4 直译 |
| `screen_scale = TILESCALE*TILESIZE*mouse_damping*CANV_SCALE` | keep | 公式直译 |
| Shadow 偏移：`VT.{x,y} -= parrallax * h; VT.scale *= (1-0.2*h)` | keep | 临时改 transform 画完恢复 |
| `_send` 自定义 uniform 路径（ref_table/func） | rewrite | C++ 端 `std::function` 或 lambda |
| `vortex` 单独分支 | keep | 直译 |
| **dissolve.fs**（默认 fallback） | **keep** | Phase 2 优先；所有共享 uniform 靠它验证 |
| **10 个卡牌特效 shader**（foil/holo/...） | keep | Phase 2 选 1 个验证；GLSL 330 重写（见 shader-uniforms.md） |
| `CRT.fs` 后处理 | defer | Phase 7+ |
| `background.fs` | defer | 静态 ClearBackground 先用 |
| `flame / splash / flash / gold_seal / skew` | defer | 各自非核心 |
| `G.SHADERS = {}` 扫目录加载 | rewrite | `unordered_map<string, Shader>` + `LoadShader` |
| 13 个 atlas 表（`game.lua:5660+`） | rewrite | JSON 配置或 `constexpr` 表 |
| `animation_atli` (blind_chips / shop_sign) | defer | 动画系统不做 |
| `Sprite.video` | drop | raylib 没原生视频 |

## 4 · Particles（Phase 5 / 7）

`balatro/04-particles.md`

| Lua | 决策 | 备注 |
|--|--|--|
| `Particles:init` 配置驱动 | keep | `struct ParticleConfig` 直译 |
| `attach`（major + alignment `cm`） | rewrite | 显式父子关系 |
| `padding / fill / created_on_pause` | keep / keep / defer | |
| `timer_type` (`REAL` / `TOTAL`) | keep | 两个 timer 源 |
| `pulse_max` ≤ 20 + `pulsed` 计数 | keep | 硬上限保留 |
| `vel_variation` 公式 `speed * (var*rand + (1-var)) * 0.7` | keep | 抄 |
| `initialize` 快进 60 帧 | keep | 步长 `15/60` |
| spawn 单帧上限 20 | **keep** | 防低帧率灾难性堆积 |
| `fill && abs(T.r)<0.1` 那段非标准旋转 | **keep** | **不要改写**为标准旋转矩阵 |
| 粒子结构（dir/facing/age/velocity/r_vel/e_prev/e_curr/scale/offset） | keep | `struct Particle` 直译 |
| scale 三角包络 + e_vel 指数追赶公式 | **keep** | 公式逐字抄 |
| 位置更新 `sin→x, cos→y` | **keep** | love2d y 向下，不要"修正" |
| `velocity *= (1 - 0.07*dt)` | **keep** | 0.07 是 Balatro 标定常数 |
| `Particles:fade` 走 `E_MANAGER` | rewrite | 自己写 tween（先直接改 fade_alpha） |
| draw 单色实心方块 | keep | `DrawRectanglePro` + 旋转 + 颜色 |

## 5 · UI（全部 drop / rewrite，Phase 6+）

`balatro/05-ui-system.md`

| Lua | 决策 | 备注 |
|--|--|--|
| `UIBox` / `UIElement` / `G.UIT.*` 全套 | **drop** | 不存在 |
| `set_parent_child` / `calculate_xywh` / `set_wh` / `set_alignments` | drop | raylib `MeasureTextEx` + 手算 |
| `UI_definitions.lua` 16918 行表 | drop | 每个 UI 一个 C++ 函数 |
| `ref_table / ref_value / func` 数据绑定 | drop | immediate-mode 每帧直读 |
| Card `use_button / sell_button` | rewrite | hover 时 immediate-mode 画两按钮 |
| Card `alert` 红点 | rewrite | `DrawCircle`，~5 行 |
| Card `h_popup` hover 详情 | rewrite | hover 时画 popup box，~30 行 |
| Card `focused_ui` 手柄聚焦 | defer | Phase 8+ |
| HUD（chips/mult/hands/discards/score/target/dollars/ante/round） | rewrite | `DrawTextEx`，~30 行（见 hud-rendering.md） |
| Blind select / Shop / Settings menu | defer | Phase 6+ raygui |
| `tooltip` / `detailed_tooltip` 系统 | rewrite | 简化版 |
| 字体度量（FONTSCALE / squish） | rewrite | `MeasureTextEx`，不抄 squish |

## 6 · Draw Pipeline（Phase 4-6）

`balatro/06-draw-pipeline.md`

| Lua | 决策 | 备注 |
|--|--|--|
| 三层 canvas（`CANVAS` → `AA_CANVAS` → 默认 fb） | keep | raylib `RenderTexture2D` ×2 |
| `G.CANV_SCALE` 内部超采样 | keep | 通常 = 2 |
| `Game:draw()` 18 阶段顺序 | **keep** | 每阶段一段 if/for；**保留顺序** |
| `not v.parent` 过滤 | keep | hierarchy root 标志 |
| `dragging.target` 单独画到阶段 14 | keep | 思路保留 |
| `focused.target` Card 阶段 15 | defer | 手柄 Phase 8+ |
| `OVERLAY_MENU / OVERLAY_TUTORIAL / screenwipe` | defer | menu 系统未起 |
| `G.I.POPUP` 阶段 16 | rewrite | hover popup 直接 immediate-mode 画在主循环末尾 |
| `G.I.ALERT` 阶段 13 | defer | 成就系统未起 |
| `Card:draw(layer)` 单次内分 shadow + card | **keep** | 不外层两遍循环 |
| `G.shared_shadow` 全局变量 | rewrite | 用本地变量传给 `DrawShadow` |
| `shadow_height` 三档（0.35 / 0.04 / 0.1） | keep | 取值逻辑直译 |
| `translate_container()` | keep | `BeginRoom() / EndRoom()` helper |
| CRT 后处理 | defer | Phase 7+ |
| `crt_intensity = 0.16 * crt/100` / `crt *= 0.3` 削弱 | keep | 手感常数 |
| `scanlines = canvasHeight * 0.75 / CANV_SCALE` | keep | 公式直译 |
| `timer_checkpoint` profile | rewrite | 自定义 Profiler，段名沿用 |

## 按 Phase 汇总（实施顺序）

| Phase | 范围 | 关键 keep 项 |
|--|--|--|
| 2 | asset 同步 + shader 适配 | dissolve.fs 移植到 GLSL 330；选 1 个卡牌特效 shader 验证（建议 holo） |
| 3 | Movable T/VT | exp ease 公式 + 常数 50/60/190；juice / pinch / shadow 字段 |
| 4 | Sprite + 一张卡 | atlas + Quad；prep_draw transform 链；draw_steps 多 pass 框架 |
| 5 | CardArea + 手牌弧形 | hand 弧形 / play 直线 / joker 三档公式；Card flip + 4 子 Sprite |
| 6 | 一轮玩法循环 | parse_highlighted + poker 评估器；HUD immediate-mode；CardArea 切区动画 |
| 7+ | Joker / Tarot / shader 全套 | 10 个卡牌特效 shader；deck 视差；CRT；particles 在 Joker 上 attach |

**总计 keep: ~50 项**（核心公式、字段、阶段顺序）；**rewrite: ~30 项**
（容器 / UI / 全局表 / 反射）；**drop: ~10 项**（Object 体系 / UIBox / 视频）；
**defer: ~25 项**（手柄 / 成就 / 复杂 menu / 不影响 MVP 的 area type）。
