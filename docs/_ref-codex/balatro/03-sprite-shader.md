Source: ref-balatro/engine/sprite.lua:5-243, card.lua:6037-6382
Last reviewed: 2026-05-01

# 03 - Sprite / shader

## Atlas + quad

`Sprite` uses atlas slicing via quad coordinates.

## draw_steps as render pass chain

`draw_steps` defines a pass list (shader + params).
Most card visual effects are implemented as pass stacking.

## draw_shader shared uniforms

Common uniforms include:

- `mouse_screen_pos`
- `screen_scale`
- `hovering`
- `dissolve`
- `time`
- `texture_details`
- `image_details`

Keep these names consistent in the raylib port.

## Port checklist

- keep: atlas slicing + pass-chain model
- keep: shared uniform semantics
- rewrite: Love2D shader API to raylib API