# Notes & design docs

`docs/` 是这个项目的「第二份事实来源」。代码说"现在是怎么实现的"；这里的笔记说"为什么这样实现 + Balatro 原版怎么做的"。

## 目录

```
docs/
├── README.md                         # 这份索引
├── roadmap.md                        # MVP 实现进度 + 经验教训
├── balatro/                          # ref-balatro 源码精读
│   ├── 01-object-node-moveable.md
│   ├── 02-cardarea-card.md
│   ├── 03-sprite-shader.md
│   ├── 04-particles.md
│   ├── 05-ui-system.md               # 仅作了解，不移植
│   └── 06-draw-pipeline.md
└── architecture/                     # 我们 C++ 实现的设计决策
    ├── port-decisions.md             # 哪些原样移植 / 改写 / 丢
    ├── ecs-vs-oo.md
    ├── shader-uniforms.md
    ├── hud-rendering.md
    └── asset-pipeline.md
```

## 写作约定

- 每篇笔记顶部 frontmatter：
  ```markdown
  ---
  source: ref-balatro/<file>:<line-range>
  last reviewed: YYYY-MM-DD
  ---
  ```
- 单篇 ≤300 行，超了就拆。
- Lua 引用片段每段 ≤4 行，加文件:行号。
- 末尾 "Port checklist" 小节列出 keep / rename / drop。

## 怎么用

- 写代码前：先看对应 `balatro/*.md` 笔记，然后看 `architecture/port-decisions.md` 确认决策。
- 写代码时：碰到不确定的字段、行为，**先回去读 lua 原文**，更新笔记，再写代码。
- 写完一个 Phase：去 `roadmap.md` 勾选，并补一段 "经验教训"（踩过哪些坑、哪个公式没看懂折腾了多久）。

## 与项目其他文档的关系

- 项目根 `CLAUDE.md`：给 AI 助手的项目介绍 + ref 路径。
- 项目根 `README.md`：构建 / 运行 / 工具链。
- `docs/`（本目录）：领域知识 + 设计决策。
