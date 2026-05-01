Source: ref-balatro/engine/particles.lua:5-207
Last reviewed: 2026-05-01

# 04 - Particles

`Particles` 继承自 `Moveable`，是可附着、定时发射的轻量粒子系统。

## 初始化结构

关键参数：

- `attach`: 绑定到某个 major 对象
- `fill`: 是否在区域内随机出生
- `timer` / `timer_type`: 发射频率与时钟来源
- `lifespan` / `speed` / `scale` / `colours`
- `max` / `pulse_max`: 粒子上限

## update：按 timer 追时钟发射

每帧通过 while 循环补发粒子，并限制单帧最多新增 20 个。

## move：更新生灭和位移

先走 `Moveable.move(self, dt)`，再更新 age/scale/velocity。
超寿命粒子会被移除。

## draw：几何矩形粒子

每个粒子用旋转小方块绘制，并叠加透明度衰减。

## Port checklist

- keep: timer + lifespan + fill 的简单模型
- rewrite: fade 事件系统
- defer: 复杂风格化效果