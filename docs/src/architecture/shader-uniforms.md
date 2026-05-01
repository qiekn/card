---
source: ref-balatro/resources/shaders/*.fs（19 个文件）
---

# Shader Uniforms

19 个 shader 的 uniform 全表 + LÖVE GLSL 110 → raylib GLSL 330 的移植
对照。Phase 2 选 `dissolve` 优先实现，所有共享 uniform 靠它验证。

## 1 · LÖVE GLSL 包装的差异

LÖVE 把 GLSL 包装成"effect 函数"风格，移植时要还原成标准 `main()`。
关键映射：

| LÖVE 写法 | GLSL 330 替代 |
|--|--|
| `extern <type> name;` | `uniform <type> name;` |
| `MY_HIGHP_OR_MEDIUMP` 宏 | 不需要，330 不区分 |
| `number` | `float`（LÖVE 别名） |
| `Image` | `sampler2D` |
| `Texel(sampler, uv)` | `texture(sampler, uv)` |
| `vec4 effect(vec4 colour, Image tex, vec2 uv, vec2 screen_uv)` | `void main()`，自己定义 `out vec4 fragColor` |
| `vec4 position(mat4 mvp, vec4 vp)`（vertex） | 标准 `void main()` 写到 `gl_Position` |
| `gl_FragColor` | 自定义 `out vec4 fragColor` |
| `love_ScreenSize` | 自传 `uniform vec2 screen_size` |
| `#ifdef VERTEX / FRAGMENT` 单文件双 stage | raylib 走 `LoadShader(vsPath, fsPath)`，**拆成 .vs + .fs 两个文件** |

因为 raylib 的 `LoadShader` 接受顶点 + 片元两个独立文件，移植时**必须拆**：
原 `.fs` 里 `#ifdef VERTEX` 段抽到 `.vs`，剩余抽到 `.fs`。

`Texel(tex, uv)` 在 LÖVE 里默认翻转 y（贴图原点左上）；raylib `texture()`
默认原点左下。直译可能需要 `uv.y = 1.0 - uv.y`——**Phase 2 要 case-by-case
验证**，不要预先一律翻。

## 2 · 共享 uniform（10 个卡牌特效 shader 都有）

| uniform | 类型 | 由谁送 | 含义 |
|--|--|--|--|
| `mouse_screen_pos` | vec2 | `Sprite:draw_shader` 标准分支 | `tilt_var.{mx,my} * CANV_SCALE` 或鼠标位置 |
| `screen_scale` | float | 同 | `TILESCALE * TILESIZE * mouse_damping * CANV_SCALE` |
| `hovering` | float | 同 | `hover_tilt * tilt_shadow`，shadow 时为 0 |
| `dissolve` | float | 同 | `abs(draw_major.dissolve)`，0..1 |
| `time` | float | 同 | `123.33412 * (ID/1.14212) % 3000`——**基于 ID 的稳定相位** |
| `texture_details` | vec4 | 同 | `(sprite_pos.x, sprite_pos.y, atlas.px, atlas.py)` |
| `image_details` | vec2 | 同 | atlas 整图 (w, h) 像素 |
| `shadow` | bool | 同 | `not not _shadow_height` |
| `burn_colour_1` | vec4 | 同 | `dissolve_colours[1]` 或 `G.C.CLEAR` |
| `burn_colour_2` | vec4 | 同 | `dissolve_colours[2]` 或 `G.C.CLEAR` |

详见 `balatro/03-sprite-shader.md` §5。

### 2.1 共享片段函数

10 个卡牌特效 shader **全部内嵌一份相同的 `dissolve_mask(tex, uv1, uv2)`**
（约 35 行代码，`dissolve.fs:15-50`）。这是溶解出场 + burn 边缘色的实现，
依赖共享的 `dissolve / texture_details / image_details / shadow /
burn_colour_*`。

移植策略：把 `dissolve_mask` 抽到 `common.glsl` include，10 个 shader
`#include` 一次。**省 ~350 行重复代码**。

### 2.2 共享 vertex shader

10 个卡牌特效 shader 的 `#ifdef VERTEX` 段也是相同代码（`dissolve.fs:73-86`）：

```glsl
vec4 position(mat4 mvp, vec4 vp) {
  if (hovering <= 0.0) return mvp * vp;
  float mid_dist = length(vp.xy - 0.5*love_ScreenSize.xy)
                   / length(love_ScreenSize.xy);
  vec2 mouse_offset = (vp.xy - mouse_screen_pos.xy) / screen_scale;
  float scale = 0.2 * (-0.03 - 0.3*max(0., 0.3-mid_dist))
              * hovering * pow(length(mouse_offset), 2.0)
              / (2.0 - mid_dist);
  return mvp * vp + vec4(0, 0, 0, scale);
}
```

10 个文件**复制粘贴一模一样**。移植拆出 `common.vs` 共用即可。

注意 `+ vec4(0, 0, 0, scale)`——**修改的是 vertex.w**（齐次 W），不是 z。
hover 时让卡片"凸起"，靠透视除法（GLSL 自动 `xyz/w`）实现"摄像机拉近"
错觉。这是非常 hack 的小聪明，**直译，不要改成改 z**。

## 3 · 各 shader 完整 uniform 列表

### 3.1 卡牌特效（10 个，共享 §2 + 自己一个 vec2）

| shader | 自定义 | lua 端送值（如有） |
|--|--|--|
| `foil` | `uniform vec2 foil` | 通过 `_send` 自定义 |
| `holo` | `uniform vec2 holo` | 通过 `_send` 自定义 |
| `polychrome` | `uniform vec2 polychrome` | 通过 `_send` 自定义 |
| `negative` | `uniform vec2 negative` | 通过 `_send` 自定义 |
| `negative_shine` | `uniform vec2 negative_shine` | 通过 `_send` 自定义 |
| `hologram` | `uniform vec2 hologram` | 通过 `_send` 自定义 |
| `voucher` | `uniform vec2 voucher` | 通过 `_send` 自定义 |
| `booster` | `uniform vec2 booster` | 通过 `_send` 自定义 |
| `debuff` | `uniform vec2 debuff` | 通过 `_send` 自定义 |
| `played` | `uniform vec2 played` | 通过 `_send` 自定义 |

`vec2` 的 `.r/.g` 在片元里当作"随机相位 + 强度"用。比如 holo 的 `holo.y`
是 hue 偏移种子，`holo.x`（实际是 `holo.r`）是动画相位
（`holo.fs:103, 115`）。**每张 Edition 卡 init 时 `random()` 一次写入这
两个分量，整个生命周期不变**——所以同一张 holo 卡的图案稳定，但卡和卡之
间各不一样。

### 3.2 dissolve（默认 fallback）

```glsl
uniform float dissolve;
uniform float time;
uniform vec4  texture_details;
uniform vec2  image_details;
uniform bool  shadow;
uniform vec4  burn_colour_1;
uniform vec4  burn_colour_2;
uniform vec2  mouse_screen_pos;
uniform float hovering;
uniform float screen_scale;
```

**没有**自定义 vec2——它是其它 shader 共享的"基础"。`Sprite:draw_shader`
的 `_shader == nil` 时默认走 dissolve。Phase 2 必须先实现。

### 3.3 vortex

```glsl
uniform float vortex_amt;   // = G.TIMERS.REAL - G.vortex_time
```

**绕过共享 uniform 路径**（`sprite.lua:104-105` 单独分支），只送 vortex_amt
一个值。用在卡牌"被 vortex 吸走"的过场——MVP 不需要，defer。

### 3.4 flame

```glsl
uniform float time;
uniform float amount;
uniform vec4  texture_details;
uniform vec2  image_details;
uniform vec4  colour_1;
uniform vec4  colour_2;
uniform float id;
```

走 `_send` 自定义路径，没自动共享 uniform。Joker 卡上的火焰用。

### 3.5 flash

```glsl
uniform float time;
uniform float mid_flash;
```

一次性闪屏（出 boss / 失败时）。

### 3.6 gold_seal

```glsl
uniform vec4 gold_seal;
```

封蜡卡角的金色，单 uniform。

### 3.7 skew

```glsl
uniform vec2  mouse_screen_pos;
uniform float hovering;
uniform float screen_scale;
```

UI 元素 hover 倾斜，**没有 dissolve / time / texture_details**——是
共享 vertex shader 的"裸版"。

### 3.8 splash（标题动画）

```glsl
uniform float time;
uniform float vort_speed;
uniform vec4  colour_1;
uniform vec4  colour_2;
uniform float mid_flash;
uniform float vort_offset;
```

### 3.9 background（场景背景）

```glsl
uniform float time;
uniform float spin_time;
uniform vec4  colour_1;
uniform vec4  colour_2;
uniform vec4  colour_3;
uniform float contrast;
uniform float spin_amount;
```

### 3.10 CRT（后处理，详见 06 笔记 §4）

```glsl
uniform float time;
uniform vec2  distortion_fac;        // (1+0.07*crt%, 1+0.10*crt%)
uniform vec2  scale_fac;             // (1-0.008*crt%, 1-0.008*crt%)
uniform float feather_fac;           // 0.01
uniform float noise_fac;             // 0.001 * crt%
uniform float bloom_fac;             // bloom-1
uniform float crt_intensity;         // 0.16 * crt%
uniform float glitch_intensity;      // 0
uniform float scanlines;             // canvasH * 0.75 / CANV_SCALE
uniform vec2  mouse_screen_pos;
uniform float screen_scale;
uniform float hovering;
```

`crt%` 即 `G.SETTINGS.GRAPHICS.crt / 100`（百分比，0..1）。所有 `* crt%`
的式子都是为了"settings 滑块在 0..100 范围内"提供细粒度控制。**这些
系数是手感常数，逐字保留**。

## 4 · 移植优先级

| Shader | Phase | 备注 |
|--|--|--|
| `dissolve` | 2 | **最先**：共享 uniform 全靠它验证；同时打通 `common.glsl`/`common.vs` 拆分 |
| `holo` | 2 | 第一个卡牌特效；验证 `_send` vec2 路径 |
| `foil` | 7 | 同 holo 套路，验证后期可批量 |
| `polychrome` / `negative` / `negative_shine` | 7 | 同 |
| `voucher` / `booster` / `hologram` | 7 | 同 |
| `debuff` / `played` | 7 | 状态特效 |
| `flame` | 7 | Joker 火焰，依赖 particles + `_send` |
| `vortex` | 7 | 过场特效，单独分支 |
| `gold_seal` | 7 | 简单 |
| `flash` | 7 | 简单 |
| `splash` | 8+ | 标题画面，MVP 不需要 |
| `background` | 8+ | 静态背景先 ClearBackground |
| `CRT` | 8+ | 后处理，最后做 |
| `skew` | 8+ | UI hover 视觉，可与 immediate-mode UI 一起 defer |

## 5 · 移植清单

实施 Phase 2 时检查：

- [ ] `common.glsl`（dissolve_mask + 共享 uniform 声明）抽出来
- [ ] `common.vs`（hovering vertex 推齐次 W）抽出来
- [ ] `dissolve.fs / dissolve.vs` 移植 + 离屏渲染验证
- [ ] `UploadStdUniforms(shader, ctx)` C++ helper（送 §2 那 10 个）
- [ ] `holo.fs / holo.vs` 移植 + 一张 holo Joker 渲染验证
- [ ] `_send` 自定义 uniform 上传路径打通
- [ ] `texture` y 翻转 case 验证（Texel vs texture）
- [ ] 至少 1 张 atlas（推荐 `Joker`）能正确切片 + dissolve 出现
