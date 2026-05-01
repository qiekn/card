---
source: src/engine/transform.h
---

# Transform

我们需要一个地方放 position / size / rotation / scale。最简单的形状是
六个 float 摞一起：

**Listing 1**: `src/engine/transform.h`

```cpp
namespace engine {
struct Transform {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
  float r = 0.0f;
  float scale = 1.0f;
};
}  // namespace engine
```

就这样。没方法，没继承——Balatro 的 lua 端就是个 6-key table（`Node.T`），
我们直译。每个 `Movable` 持有两份：`T`（目标）和 `VT`（可见）。游戏代码
设 `T` 是瞬时的，引擎让 `VT` 每帧 ease 过去。

## 为什么正好六个字段

- `(x, y)` 是 position（**game units**，不是像素 —— 像素换算留到画的那一刻）
- `(w, h)` 是 size，跟 position 同坐标系
- `r` 是 rotation（**radians**，跟 raylib API 的 degrees 不一样，画的时候
  乘 `57.2957795` 转一下）
- `scale` 是 **uniform** scale —— Balatro 没分 sx/sy（除非用 `pinch`，
  那是 Movable 的另一套机制）

少一个嫌不够（譬如做 flip 时改 `pinch.x` 让 `VT.w` 收到 0），多一个浪费
（Balatro 跑了上千张牌都没需要分 sx/sy）。**这是一个尺寸定下来就别再
动的 struct**。

## 为什么单独一个 header

把它塞 `Movable` 里也能跑。我们没那么做，因为 `Sprite` / `Card` / 后面
要写的 `CardArea` 都会用到 Transform 的别处实例（譬如 `align_to`、
碰撞箱 `CT` defer 不做但留口）—— 单独 header 让它能 forward declare 也
能值传递。一个 file，13 行，包出去。

## 为什么放 namespace 里

raylib 也定义了 `Transform`（`raylib.h:451` 左右，骨骼蒙皮用的 4×4 矩阵
+ 四元数）。我们跟它没关系但名字撞了。任何 include `raylib.h` 又 include
我们 `transform.h` 的 .cpp 都会立刻 redefinition error —— 第一次接 `engine::`
namespace 就因为这个。

`engine::Transform` 念起来稍长但安全，`Movable` 下面会跟着进同一个
namespace，所以 `Movable::T()` 返回的也是 `engine::Transform&`。

## Caveats

- **没 `==`，没 `<<`**：要打日志时手写 `printf("%.1f %.1f", t.x, t.y)`。
  之后真需要再加。
- **默认 scale = 1.0**，其它字段默认 0 —— 别忘了构造 Movable 时 `HardSetT`
  把 `(x, y, w, h)` 写一次，不然 `VT.w / VT.h = 0`，画出来啥也看不见。