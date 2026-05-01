Source: ref-balatro/game.lua:8328-8640, cardarea.lua:325-510
Last reviewed: 2026-05-01

# HUD rendering

MVP 的 HUD 在 FBO 内自绘，不依赖 UIBox。

## 推荐层级

1. 背景
2. CardArea
3. Card
4. dragging/focused card
5. HUD
6. 最终 blit 到窗口

ImGui 只用于开发面板，不参与游戏 HUD。

## HUD 最小集

- 当前分数
- 目标分数
- 剩余手数
- 剩余弃牌数

## Port checklist

- keep: 画布内分层思路
- rewrite: HUD 改轻量自绘
- drop: UIBox attention/tooltip 链