---
source: 项目实现侧文档（B 文档）入口
---

# 项目实现

> **位置**：与 `balatro/`（源码精读）和 `architecture/`（移植决策）平级。
> 这一段写**我们的 C++ 实现**——代码长什么样、为什么那样写。

## 与 A 文档的关系

| 段 | 写什么 | 不写什么 |
|--|--|--|
| `balatro/` | Balatro lua 原作怎么实现 | C++ 代码 / 类继承图 |
| `architecture/` | 移植决策（keep / rewrite / drop） | 具体 C++ 类签名、字段细节 |
| `project/`（本段）| 我们的 C++ 代码组织、每个 system 的接口与字段 | 不重复 lua 行为分析 |

A 文档（`balatro/` + `architecture/`）已在 Phase 1 冻结：写 lua 行为
**+ 决策**，不放 C++ 代码示例。Phase 2 起开始写 C++，C++ 类层次、
具体字段、调用约定全部进 `project/` 这一段。

## 当前章节

- [Code Layout](./code-layout.md) — `src/` 现有骨架：Game / Layer / LayerStack
- [Build System](./build-system.md) — CMake + clang + libc++ 硬约束的来由

## 写作约定

继承自 A 文档（`docs/src/README.md` §写作约定），有几条调整：

- **frontmatter** 的 `source:` 字段：A 文档指向 `ref-balatro/<file>:<line>`；
  B 文档指向我们仓内 `src/<file>` 或子系统名。
- **port checklist 四档**（keep / rewrite / drop / defer）只在 A 文档用；
  B 文档结尾用"接口边界"或 caveats 代替。
- **常用工程术语保留英文**：pipeline / shader / hook / handle / backend /
  viewport / vertex / texture / atlas / batch / event / overlay 等不翻译，
  代码 / API / stack 都是英文，混读时英文反而流畅。
- **叙事风格**：讲"系统从无到有怎么搭"型笔记（transform.md / movable.md
  / sprite.md / card.md ...）参考 *Ray Tracing in One Weekend* —— 线性
  增量、从代码片段出发引出概念、第一人称 we、章节短小、能跑就贴运行
  结果。reference 性质的（code-layout.md / build-system.md）保留表格 +
  bullet OK。

其它一致：
- 单篇 ≤300 行，超了拆
- 中文解说，英文代码 / commit / 文件名
- fenced code block ≤75 字符（避免 mdBook 横向滚动）
- 关键代码片段 ≤4 行 + 行号

## 何时新增笔记

每个 Phase 引入一个新子系统就加一篇。命名约定 `<system>.md`，例如：

| Phase | 预期笔记 |
|--|--|
| 3 | `transform.md`、`movable.md`（T / VT / Move / HardSetT / JuiceUp） |
| 4 | `sprite.md`（atlas + quad slicing） |
| 5 | `card.md`、`cardarea.md` |
| 6 | `state-machine.md`、`scoring.md` |
| 7+ | `shader-loader.md`（`#include` 预处理）、`particles.md` |

每篇 ≤300 行；超过就按子主题拆（`movable-juice.md` / `movable-collision.md`
等）。
