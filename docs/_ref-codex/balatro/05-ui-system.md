Source: ref-balatro/engine/ui.lua:1-260, game.lua:8392-8440
Last reviewed: 2026-05-01

# 05 - UI system

这篇只做理解，不做移植。

## UIBox 本质

UIBox 是声明式 UI 树 + 布局求解器，不是单一控件。
初始化会递归计算节点尺寸、对齐和关系。

## 复杂度来源

- 文本测量
- 容器约束（padding/min/max）
- 行列组合递归
- 动态重算

## 为什么 MVP 不移植

- 体量大，和主玩法耦合低
- 移植成本高
- raylib + ImGui 足够覆盖 MVP 需求

## Port checklist

- keep: UI 与玩法分层思想
- drop: UIBox 原始实现
- rewrite: MVP 只保留轻量 HUD/按钮