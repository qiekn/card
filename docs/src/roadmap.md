# Roadmap

记录 MVP 路线图的进度。每个 Phase 完成后勾选 + 写"经验教训"。

完整 plan: `~/.claude/plans/balatro-fluttering-mccarthy.md`。

## Phase 1 — 研究笔记

无 C++ 改动，纯文档。完成的 gate：6 篇 `balatro/*.md` + 5 篇 `architecture/*.md` 全部存在；`port-decisions.md` 三栏完整；`shader-uniforms.md` 覆盖所有 Balatro shader 的 uniform。

- [x] `balatro/01-object-node-moveable.md`
- [x] `balatro/02-cardarea-card.md`
- [x] `balatro/03-sprite-shader.md`
- [x] `balatro/04-particles.md`
- [x] `balatro/05-ui-system.md`
- [x] `balatro/06-draw-pipeline.md`
- [x] `architecture/port-decisions.md`
- [ ] `architecture/ecs-vs-oo.md`
- [ ] `architecture/shader-uniforms.md`
- [ ] `architecture/hud-rendering.md`
- [ ] `architecture/asset-pipeline.md`

### 经验教训

_(每勾掉一项就在这里补一段：哪些 lua 行没看懂折腾了多久 / 哪个公式实际意义和我猜的不一样)_

## Phase 2 — 工程基础设施

- [ ] `tools/update-balatro-assets.sh` 写完 + 验证 dry-run
- [ ] `.gitignore` 加 `/assets/balatro/`
- [ ] `README.md` 加 "First-time asset setup" 段
- [ ] 1 个 shader（推荐 holo.fs）改成 raylib 330 编译通过

### 经验教训
_(待填)_

## Phase 3 — 引擎抽象 (Movable T/VT)

- [ ] `src/engine/transform.h`
- [ ] `src/engine/movable.h/.cpp` (Move / HardSetT / JuiceUp)
- [ ] 键盘 1/2/3 切换 T.x，VT.x 平滑跟随的 demo

### 经验教训
_(待填)_

## Phase 4 — Sprite + 一张卡

- [ ] `src/engine/sprite.h/.cpp` (atlas + quad slicing)
- [ ] viewport 中央渲染一张卡，atlas quad 边界正确

### 经验教训
_(待填)_

## Phase 5 — CardArea + 手牌弧形

- [ ] `src/game/card.h/.cpp` (Card 字段框架)
- [ ] `src/game/cardarea.h/.cpp`，type "hand" / "play" 公式
- [ ] 8 张卡弧形排列 demo，加/减卡平滑重排

### 经验教训
_(待填)_

## Phase 6 — 一轮玩法循环

- [ ] 状态机 DRAW_TO_HAND → SELECTING_HAND → PLAY_HAND → PLAYING_HAND → ROUND_FINISHED
- [ ] 出牌 → 扑克手牌识别 → chips × mult 记分
- [ ] HUD 自绘 (DrawTextEx) 显示分数/目标分/手数/弃牌数

### 经验教训
_(待填)_

## Phase 7 — Joker / Planet / Tarot + Shader 起点

留作未来 plan，本阶段不展开。
