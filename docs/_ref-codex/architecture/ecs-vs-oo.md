Source: ref-balatro/engine/{node,moveable}.lua, cardarea.lua, card.lua
Last reviewed: 2026-05-01

# ECS vs OO

建议采用“OO 外观 + ECS 存储”混合方案。

## 映射

- 行为接口：`Card/CardArea/Sprite`
- 数据组件：`Transform/VisibleTransform/MotionState/...`

## 系统切分

1. MoveSystem
2. LayoutSystem
3. InputSystem
4. RenderSystem

## 选择理由

- 不纯 ECS：公式拆分成本太高
- 不纯 OO：全局索引维护成本太高

## Port checklist

- keep: 领域对象边界
- rewrite: 全局索引迁到 ECS 查询
- defer: 高级角色绑定系统