Source: ref-balatro/engine/sprite.lua:75-156, game.lua:8562-8583
Last reviewed: 2026-05-01

# Shader uniforms

## 通用 uniform

| 名称 | 含义 | raylib 类型 |
|--|--|--|
| `mouse_screen_pos` | 鼠标画布坐标 | `SHADER_UNIFORM_VEC2` |
| `screen_scale` | 缩放 | `SHADER_UNIFORM_FLOAT` |
| `hovering` | 倾斜强度 | `SHADER_UNIFORM_FLOAT` |
| `dissolve` | 溶解进度 | `SHADER_UNIFORM_FLOAT` |
| `time` | 时间输入 | `SHADER_UNIFORM_FLOAT` |
| `texture_details` | 纹理定位参数 | `SHADER_UNIFORM_VEC4` |
| `image_details` | 图像尺寸参数 | `SHADER_UNIFORM_VEC2` |
| `burn_colour_1/2` | 溶解颜色 | `SHADER_UNIFORM_VEC4` |
| `shadow` | 阴影 pass 标记 | `SHADER_UNIFORM_INT` |

## CRT 专有

`distortion_fac`、`scale_fac`、`noise_fac`、`scanlines` 等。

## Port checklist

- keep: uniform 命名语义
- rewrite: `send` API 到 `SetShaderValue`
- defer: 非 MVP 视觉微调