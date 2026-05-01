# PLAN.md — 会话接力便签

> 给下一个 Claude Code 会话的开场提示。读完这个 + `docs/src/roadmap.md` 就能接着干。
>
> 完整路线图：`~/.claude/plans/balatro-fluttering-mccarthy.md`。

## 当前状态（2026-05-01）

- 分支：`dev`（领先 origin 多个 commit）
- 项目阶段：**Phase 1 完成 → 进入 Phase 2**
- mdBook 已 working：`docs/book.toml`，src = `docs/src/`，CI 部署到 GitHub Pages（`.github/workflows/`）。
- ref-balatro 源码在 `./ref-balatro/`（已 `.gitignore`，是 Balatro 1.0.1o-Full 的 love2d 源）。

## Phase 1 完成情况（11/11）

balatro 笔记 6 篇 + architecture 笔记 5 篇全部 ✅，每篇独立 commit。详见 `docs/src/roadmap.md`。

入口（mdbook 站点已可读）：

- `docs/src/balatro/01-06-*.md` — Balatro 引擎拆解（写作风格样板：01）
- `docs/src/architecture/{port-decisions, ecs-vs-oo, shader-uniforms, hud-rendering, asset-pipeline}.md` — 移植决策

## 写笔记的硬约定（如未来还要补 / 修笔记）

- **frontmatter**：`source: ref-balatro/<file>:<line-range>`（不写 `last reviewed`，用户已去掉这条 —— 见 commit `07928b7`/`3a1cd87`）
- **结构**：字段表 → 关键 lua 片段（≤4 行 + 行号）→ port checklist（keep / rewrite / drop / defer 四档）
- **长度**：≤300 行，超了就拆
- **语言**：技术名词、代码、commit 用英文；解说性段落中文
- **公式与常量必须保留**：例如 01 笔记的 `math.exp(-50*dt)` / K = 50/60/190 —— codex 把这些裁掉过，导致整篇笔记被退回
- **fenced code block 行 ≤ 75 字符**（避免 mdbook 横向滚动；memory 里也有）
- **balatro 笔记里不写 C++ 代码示例 / 实现细节**——port checklist 给简短映射即可，C++ 实现细节留给项目文档 B（见 docs_split_plan memory）

## 用户偏好（违反过会被退回重做）

1. **中文回复**（代码/commit/文件内容仍英文）
2. **commit 粒度尽可能小**——一个 commit 一个 logical change
3. **commit message 默认只写 subject**——subject 把意图说清楚（用户曾把多 bullet body 全删过）
4. **Conventional Commits**：`type(scope): summary`，type ∈ {feat, fix, refactor, perf, chore, docs, style, test, init}
5. **UTF-8 + LF**（无 BOM、无 CRLF）
6. **不要主动跑 cmake/build**——用户自己来
7. **绝不直接动 git 历史**（rebase/reset --hard/force push）除非用户明确要
8. **用户偏好 C++ 而非 lua**——解释 lua 时用 C++ 类比

## 文档拆分计划（重要）

未来项目文档分两份并列：

- **A：balatro 源码与架构**（即当前 `docs/src/balatro/*` + `docs/src/architecture/*`）—— Phase 1 完成，**冻结**
- **B：项目文档**（C++ 实现侧）—— Phase 2 开始陆续添加，**和 A 平级独立**

A 文档里**不放 C++ 代码示例 / 类继承图**——具体实现细节去 B 文档。
A 里只有：原作怎么做 + port 决策 + 架构权衡。

## Phase 2 启动建议

完成的 gate（roadmap.md 已写）：

- [ ] `tools/update-balatro-assets.sh` 写完 + dry-run 验证
- [ ] `.gitignore` 加 `/assets/balatro/`
- [ ] `README.md` 加 "First-time asset setup" 段
- [ ] 1 个 shader（推荐 `dissolve.fs`）port 到 raylib GLSL 330 编译通过

具体步骤参考 `docs/src/architecture/asset-pipeline.md` §3 + `architecture/shader-uniforms.md` §5。

实施 Phase 2 时**会开始动 C++**——同时建议把 B 文档的入口骨架先搭起来
（`docs/src/project/` 或新建 mdbook，按用户偏好定）。

## 文件位置参考

| 资料 | 位置 |
|--|--|
| 完整 plan | `~/.claude/plans/balatro-fluttering-mccarthy.md` |
| Memory | `~/.claude/projects/C--msys64-home-user-gamedev-remake-card/memory/` |
| codex 旧草稿（仅参考） | `docs/_ref-codex/`（不在 mdbook src 里）|
| ref-balatro 源码 | `./ref-balatro/` |
