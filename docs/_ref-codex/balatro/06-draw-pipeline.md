Source: ref-balatro/game.lua:8328-8640, cardarea.lua:325-510
Last reviewed: 2026-05-01

# 06 - Draw pipeline

`Game:draw()` 的顺序决定遮挡、焦点和后处理观感。

## 主流程

1. 进主画布 `CANVAS`
2. 背景层
3. 普通节点/对象
4. CardArea -> Card
5. overlay/menu/debug
6. dragging/focused card（最后单独画）
7. cursor 与过场
8. CRT 整屏后处理并输出

## MVP 推荐顺序

```text
BeginTextureMode(viewport)
  draw background
  draw card areas
  draw cards (exclude dragging/focused)
  draw dragging/focused cards
  draw HUD
EndTextureMode

BeginShaderMode(crt)
  draw viewport to screen
EndShaderMode
```

## Port checklist

- keep: 分层绘制 + 整屏后处理
- rewrite: 索引结构改为渲染队列
- drop: UIBox 的复杂过滤链