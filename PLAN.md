# PLAN.md — 会话接力便签

> 给下一个 Claude Code 会话的开场提示。读完这个 + `docs/src/roadmap.md` + `docs/src/balatro/01-object-node-moveable.md` 就能接着干。
>
> 完整路线图：`~/.claude/plans/balatro-fluttering-mccarthy.md`。

## 当前状态（2026-05-01）

- 分支：`dev`（领先 origin 几个 commit）
- 项目阶段：**Phase 1 — 研究笔记**，纯文档，**不写 src/ 代码**。Phase 2 之后才会动 C++。
- mdBook 已 working：`docs/book.toml`，src = `docs/src/`，CI 部署到 GitHub Pages（`.github/workflows/`）。
- ref-balatro 源码在 `./ref-balatro/`（已 `.gitignore`，是 Balatro 1.0.1o-Full 的 love2d 源）。

## Phase 1 进度

| 文档 | 状态 |
|--|--|
| `docs/src/balatro/01-object-node-moveable.md` | ✅ committed (`459956b`)，**这是写作风格的样板** |
| `docs/src/balatro/02-cardarea-card.md` | ⏳ 待重写（codex 草稿在 `_ref-codex/`） |
| `docs/src/balatro/03-sprite-shader.md` | ⏳ 待写 |
| `docs/src/balatro/04-particles.md` | ⏳ 待写 |
| `docs/src/balatro/05-ui-system.md` | ⏳ 待写（仅了解，不移植，要解释为什么） |
| `docs/src/balatro/06-draw-pipeline.md` | ⏳ 待写 |
| `docs/src/architecture/port-decisions.md` | ⏳ 待写 |
| `docs/src/architecture/ecs-vs-oo.md` | ⏳ 待写 |
| `docs/src/architecture/shader-uniforms.md` | ⏳ 待写 |
| `docs/src/architecture/hud-rendering.md` | ⏳ 待写 |
| `docs/src/architecture/asset-pipeline.md` | ⏳ 待写 |

进度勾选表：`docs/src/roadmap.md`。每篇写完都要勾上 + 在 SUMMARY 里加链接。

## 写笔记的硬约定（看 01 笔记验证）

- **frontmatter**：`source: ref-balatro/<file>:<line-range>`（不写 `last reviewed`，用户已经把这条规则去掉了 —— 见 commit `07928b7`/`3a1cd87`）
- **结构**：字段表 → 关键 lua 片段（≤4 行 + 行号）→ port checklist (keep / rewrite / drop / defer 四档)
- **长度**：≤300 行，超了就拆
- **语言**：技术名词、代码、commit 用英文；解说性段落中文
- **公式与常量必须保留**：例如 01 笔记里的 `math.exp(-50*dt)` 和 `K = 50/60/190` —— **codex 把这些都裁掉了，导致整篇笔记被退回**

## codex 污染处理（已完成）

上一次会话有 codex agent 改写了 docs，现已：

- `git restore` 复原了 `01-object-node-moveable.md` / `README.md` / `SUMMARY.md` / `roadmap.md`
- codex 起稿的 02 用 `git mv` 搬到 `docs/_ref-codex/balatro/`（保留 git history）
- 03-06 + 5 篇 architecture + `balatro-plan.md` + `AGENTS.md` 全部 mv 到 `docs/_ref-codex/`
- `_ref-codex/` 不在 mdBook src 里，不会进站点，但保留供参考

`docs/_ref-codex/README.md` 解释了这些归档草稿。**重写正式版时不要复制粘贴 codex 稿**，风格不一致。

## 用户偏好（重要，违反过会被退回重做）

1. **中文回复用户**（代码/commit/文件内容仍英文）
2. **commit 粒度尽可能小**——一个 commit 一个 logical change；不要把多个不同意图的改动合一起
3. **commit message 默认只写 subject**，不写 body；subject 必须把意图说清楚（用户曾把多 bullet body 全删过）
4. **Conventional Commits 格式**：`type(scope): summary`，type ∈ {feat, fix, refactor, perf, chore, docs, style, test, init}
5. **UTF-8 + LF**（无 BOM、无 CRLF）
6. **不要主动跑 cmake/build**——用户自己来
7. **绝不直接动 git 历史**（rebase/reset --hard/force push）除非用户明确要

## 快速定位 ref-balatro 关键代码

写 02-06 笔记时直接读这些文件 + 行号：

| 笔记 | ref-balatro 文件 | 关键行号 |
|--|--|--|
| 02 CardArea/Card | `cardarea.lua` | hand 弧形 692-722；play 直线 787-810；joker/consumeable 811-852 |
| 02 CardArea/Card | `card.lua` | 字段 1-87；children = {shadow, front, back, center} |
| 03 Sprite/Shader | `engine/sprite.lua` | atlas+quad 33-39；draw_steps 58-72；draw_shader uniform 75-156 |
| 04 Particles | `engine/particles.lua` | 全文 1-208 |
| 05 UI | `engine/ui.lua` 1-200；`functions/UI_definitions.lua` 抽样 | 声明式 `{n=G.UIT.*, config, nodes}` 树 |
| 06 Draw pipeline | `game.lua:8328-8470` | `Game:draw()`，遍历 G.I.NODE / MOVEABLE / CARD / CARDAREA / UIBOX |

## 下一步建议

1. 重写 `docs/src/balatro/02-cardarea-card.md`（沿 01 风格），完成后在 SUMMARY 里加回链接，roadmap 勾上
2. 一篇一篇推进 03-06，每篇一个 commit (`docs(balatro): add note on <topic>`)
3. 11 篇全部完成后再开 Phase 2（资产同步脚本 + shader 适配实验）

写完一篇就停下让用户过目，不要一次性把 11 篇全堆出来。
