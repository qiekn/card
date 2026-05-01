# `_ref-codex/` — codex agent 的草稿归档

这个目录里的文件是 OpenAI codex agent 在一次会话里生成的笔记/方案草稿，**不参与 mdBook 站点构建**（mdBook src 是 `docs/src/`，本目录在 `docs/_ref-codex/`），只作历史参考。

## 为什么归档而不是直接用

codex 写的版本对原稿（特别是 `01-object-node-moveable.md`）做了大幅删减——把指数 ease 公式、`G.exp_times` 数值表、Major/Minor bond 解释、22 行 port checklist 都裁掉了，换成了三五行的概念描述。这跟项目约定的"详细 + 引用 lua 公式 + 末尾 port checklist"风格相反，所以全部退回到这里。

## 目录

```
_ref-codex/
├── balatro/
│   ├── 02-cardarea-card.md       # codex 起的稿，已 commit 过 (57fe754) 后归档
│   ├── 03-sprite-shader.md       # untracked 草稿
│   ├── 04-particles.md
│   ├── 05-ui-system.md
│   └── 06-draw-pipeline.md
├── architecture/
│   ├── port-decisions.md
│   ├── ecs-vs-oo.md
│   ├── shader-uniforms.md
│   ├── hud-rendering.md
│   └── asset-pipeline.md
├── balatro-plan.md               # codex 拷贝的 plan，原始在 ~/.claude/plans/
└── AGENTS.md                     # codex 自家 LLM 提示文件，跟本项目无关
```

## 怎么用

下次重写正式版（放在 `docs/src/balatro/` 或 `docs/src/architecture/`）时：
- **可以**借鉴 codex 草稿的章节划分作为目录结构提示
- **不要**复制粘贴 — 风格不一致；按 `docs/src/balatro/01-object-node-moveable.md` 的体例重写
- 重写完成后这些草稿留着不动，最多在重写完毕后用 `roadmap.md` 标注"已被 docs/src/... 替代"
