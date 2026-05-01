Source: ref-balatro/{engine,node,moveable,cardarea,card,sprite}.lua
Last reviewed: 2026-05-01

# Port decisions

## 三栏决策

| 项目 | 决策 | 说明 |
|--|--|--|
| `T/VT` 双 transform | keep | 核心手感 |
| easing 公式 | keep | 直接移植 |
| CardArea hand/play 公式 | keep | MVP 主线 |
| Card 多层 sprite | keep | 视觉结构核心 |
| shader 多 pass | keep | edition/seal/sticker 依赖 |
| UIBox | drop | MVP 不移植 |
| `G.I.*` 索引 | rewrite | ECS / 渲染队列 |
| major/minor role | defer | 后续按需 |

## Port checklist

- keep: 手感与布局公式
- rewrite: 对象索引和 UI 体系
- defer: 非 MVP 复杂机制