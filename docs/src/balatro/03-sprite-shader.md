---
source: ref-balatro/engine/sprite.lua
---

# 03 · Sprite / Shader

`Sprite` 从 `Moveable` 派生，多了三件事：**atlas（贴图集）+ quad（切片）+
多 pass shader 调度**。它是引擎里唯一会调 `love.graphics.draw` 的类——
所有"画一张图"的请求最终都落到这里。

shader 共有 19 个（`resources/shaders/*.fs`），其中 10 个是**卡牌特效**，
共享一套 uniform 接口；其余是后处理 / 背景 / 一次性特效。这套接口是 02
笔记里 `Card.tilt_var / juice / dissolve` 等字段的"消费端"。

## 1 · Sprite 字段（`engine/sprite.lua:5-18`）

```lua
function Sprite:init(X, Y, W, H, new_sprite_atlas, sprite_pos)
  Moveable.init(self, X, Y, W, H)
  self.CT = self.VT
  self.atlas = new_sprite_atlas             -- { name, image, px, py, ... }
  self.scale = { x = atlas.px, y = atlas.py }   -- 单格像素尺寸
  self.scale_mag = math.min(scale.x/W, scale.y/H)
  self.zoom = true
  self:set_sprite_pos(sprite_pos)            -- 建 quad
  table.insert(G.I.SPRITE, self)
end
```

| 字段 | 用途 |
|--|--|
| `atlas` | 贴图集对象，含 `image`（love `Image`）+ `px/py`（单格像素）+ `name` |
| `scale = {px, py}` | atlas 单格像素尺寸（**不是缩放系数**，命名误导）|
| `scale_mag` | `min(px/W, py/H)`，用于 `draw_from` 等 |
| `sprite_pos = {x, y}` | 单格在 atlas 中的网格坐标（左上原点） |
| `sprite_pos_copy` | 上次 quad 重建时的位置；`draw_self` 里检测变化决定是否重建 |
| `sprite` | `love.graphics.Quad`（切片矩形） |
| `image_dims` | atlas 图像 (w, h) 像素，shader uniform 用 |
| `draw_steps` | 多 pass 渲染列表，可选；空就走 `draw_self` |
| `video` | 视频纹理（仅 splash/background 用） |

## 2 · Atlas + Quad 切片

### 2.1 set_sprite_pos（`sprite.lua:25-43`）

```lua
self.sprite = love.graphics.newQuad(
  self.sprite_pos.x * self.atlas.px,        -- 像素 x
  self.sprite_pos.y * self.atlas.py,        -- 像素 y
  self.scale.x, self.scale.y,               -- 切片尺寸 (px, py)
  self.atlas.image:getDimensions()          -- 整张图像尺寸
)
```

`sprite_pos.v` 存在时（动画 atlas）会随机抽一格：
`x = random(v) - 1`。这是 blind chips / shop sign 这类 spritesheet 的入口。

### 2.2 atlas 列表（`game.lua:5660-5755`）

按 `G.SETTINGS.GRAPHICS.texture_scaling`（1x/2x/4x）从同名子目录加载：

| name | 图 | px × py | 用途 |
|--|--|--|--|
| `cards_1` / `cards_2` | `8BitDeck.png` / `_opt2` | 71 × 95 | 标准扑克 rank+suit |
| `centers` | `Enhancers.png` | 71 × 95 | 牌面增强（gold/glass/...）|
| `Joker` | `Jokers.png` | 71 × 95 | 所有 Joker |
| `Tarot` | `Tarots.png` | 71 × 95 | Tarot/Planet/Spectral |
| `Voucher` | `Vouchers.png` | 71 × 95 | Voucher |
| `Booster` | `boosters.png` | 71 × 95 | Booster pack 封面 |
| `ui_1` / `ui_2` | `ui_assets.png` | 18 × 18 | UI 小图标 |
| `icons` | `icons.png` | 66 × 66 | 大图标 |
| `tags` | `tags.png` | 34 × 34 | 跳过 boss 等 tag |
| `chips` | `chips.png` | 29 × 29 | 筹码 |
| `balatro` | `balatro.png` | 333 × 216 | logo |
| `gamepad_ui` | `gamepad_ui.png` | 32 × 32 | 手柄按键 |
| `blind_chips` | `BlindChips.png` | 34 × 34，21 帧 | blind 动画（`animation_atli`）|
| `shop_sign` | `ShopSignAnimation.png` | 113 × 57，4 帧 | 商店招牌（`animation_atli`）|

**所有"卡"类资源都是 71 × 95**——这是 Balatro 的硬常量。

## 3 · 基础 draw（无 shader 路径）

### 3.1 draw 主入口（`sprite.lua:192-223`）

```lua
function Sprite:draw(overlay)
  if self.draw_steps then
    for k, v in ipairs(self.draw_steps) do
      self:draw_shader(v.shader, v.shadow_height, v.send,
                       v.no_tilt, v.other_obj, v.ms, v.mr,
                       v.mx, v.my, not not v.send)
    end
  else
    self:draw_self(overlay)
  end
  for k, v in pairs(self.children) do
    if k ~= "h_popup" then v:draw() end
  end
end
```

有 `draw_steps` 走多 pass shader，否则只画自己。**子节点永远走自己的
`draw`**——`Card` 的 4 个 Sprite 子是这样递归画出来的。

### 3.2 draw_self（`sprite.lua:158-190`）

无 shader 直接画：

```lua
prep_draw(self, 1)                          -- push + transform
love.graphics.scale(1/(scale.x/VT.w), 1/(scale.y/VT.h))
love.graphics.setColor(overlay or G.C.WHITE)
love.graphics.draw(atlas.image, sprite_quad,
                   0, 0, 0,                 -- x, y, r 已在 prep_draw
                   VT.w/T.w, VT.h/T.h)      -- pinch.x 翻牌的 0..1 缩放
love.graphics.pop()
```

### 3.3 prep_draw（`functions/misc_functions.lua:968`）

```lua
push()
scale(G.TILESCALE * G.TILESIZE)             -- 游戏单位 → 像素
translate(VT.x + VT.w/2, VT.y + VT.h/2)     -- 移到中心（含 layered_parallax）
rotate(VT.r + (rotate or 0))                -- 旋转
translate(-scale*VT.w*VT.scale/2,           -- 退回左上原点
          -scale*VT.h*VT.scale/2)
scale(VT.scale * scale)                     -- 注入 juice + 系统 scale
```

**关键**：用 `VT.w / T.w` 做最终缩放——这正是 02 笔记里 flip 用的：
`pinch.x = true` 让 `VT.w → 0`，整张卡水平压扁。

## 4 · 多 pass：draw_steps + draw_shader

### 4.1 draw_steps 定义（`sprite.lua:58-72`）

```lua
function Sprite:define_draw_steps(defs)
  self.draw_steps = EMPTY(self.draw_steps)
  for k, v in ipairs(defs) do
    self.draw_steps[#self.draw_steps + 1] = {
      shader = v.shader or "dissolve",      -- 哪个 .fs
      shadow_height = v.shadow_height,      -- 阴影偏移倍率
      send = v.send,                        -- 自定义 uniform 列表
      no_tilt = v.no_tilt,                  -- 关掉 hover 视差
      other_obj = v.other_obj,              -- 借别的 obj 的 transform
      ms, mr, mx, my,                       -- draw_from 的微调
    }
  end
end
```

一张 Joker 可以叠 `dissolve → foil → holo`：先底色 dissolve 出现，再叠
foil 金属反光，最后 holo 全息扫线。**每个 step 是一次完整的 setShader +
draw + setShader(nil)**。

### 4.2 阴影偏移（`sprite.lua:91-96, 151-155`）

```lua
if _shadow_height then
  self.VT.y = self.VT.y - draw_major.shadow_parrallax.y * _shadow_height
  self.VT.x = self.VT.x - draw_major.shadow_parrallax.x * _shadow_height
  self.VT.scale = self.VT.scale * (1 - 0.2 * _shadow_height)
end
-- ...画完后再加回去（行 151-155）
```

`Card:draw(layer="shadow")` 时给 `_shadow_height = 0.2 + ...`（`card.lua:6065`），
让阴影**偏移到光源反方向并缩小**。同时 shader uniform `shadow=true` +
`hovering=0` —— 阴影不参与视差，颜色被 shader 改成纯黑 alpha 0.3。

## 5 · 卡牌特效 shader 共享 uniform

`draw_shader` 的"标准分支"（`sprite.lua:107-138`）会送下面这些：

| uniform | 表达式 | 含义 |
|--|--|--|
| `mouse_screen_pos` (vec2) | `tilt_var.{mx,my} * G.CANV_SCALE` 或 `cursor_position * CANV_SCALE` | 鼠标屏幕坐标，hover 顶点弯曲用 |
| `screen_scale` (float) | `G.TILESCALE * G.TILESIZE * mouse_damping * G.CANV_SCALE` | 屏幕单位换算 |
| `hovering` (float) | `((shadow 且非 tilt_shadow) or no_tilt) ? 0 : hover_tilt * tilt_shadow` | hover 视差强度，0..1 |
| `dissolve` (float) | `abs(draw_major.dissolve or 0)` | 0..1，溶解出场进度 |
| `time` (float) | `123.33412 * (ID / 1.14212) % 3000` | **基于 ID 的稳定时间**，每张卡相位不同 |
| `texture_details` (vec4) | `(sprite_pos.x, sprite_pos.y, atlas.px, atlas.py)` | shader 把 uv 转成 atlas 内坐标用 |
| `image_details` (vec2) | atlas 整图像尺寸 (px) | 同上 |
| `shadow` (bool) | `not not _shadow_height` | shader 里把 rgb 置 0、alpha *0.3 |
| `burn_colour_1` (vec4) | `dissolve_colours[1] or G.C.CLEAR` | 溶解边缘第一色 |
| `burn_colour_2` (vec4) | `dissolve_colours[2] or G.C.CLEAR` | 溶解边缘第二色 |

`time` 公式注意：**不是 `G.TIMERS.REAL`**，而是 `ID/1.14212 * 123.33412
% 3000`——这样每张卡有自己稳定的相位，foil/holo 才不会"齐刷刷一致地动"。
但这意味着 `time` 不随 dt 推进，shader 的"动画感"完全靠片元里的 sin/cos
组合 + `dissolve` / `holo.r` 等其它 uniform。

`hovering > 0` 时 vertex shader 会把 vertex.w 推一个偏移
（`holo.fs:139-152`），形成 hover 时的"凸起 / 弯曲"效果。

## 6 · 各 shader 自定义 uniform 一览

按 19 个 shader 分组：

**A · 卡牌特效（10 个）**：除上面共享 uniform 外多一个 `vec2 <name>`，
shader 里用 `<name>.r/.g` 当随机相位 + 强度。

| shader | 自定义 | 用途 |
|--|--|--|
| `foil` | `vec2 foil` | 金箔 |
| `holo` | `vec2 holo` | 全息（hsl 偏移）|
| `polychrome` | `vec2 polychrome` | 彩虹 |
| `negative` | `vec2 negative` | 负片 |
| `negative_shine` | `vec2 negative_shine` | 负片高光 |
| `hologram` | `vec2 hologram` | Joker hologram |
| `voucher` | `vec2 voucher` | Voucher 边框 |
| `booster` | `vec2 booster` | Booster pack 闪 |
| `debuff` | `vec2 debuff` | 红×叉效果 |
| `played` | `vec2 played` | 出过的牌发光 |

**B · 通用 dissolve**：`dissolve.fs` 不带自定义 vec2，只有共享 uniform。
`_shader == nil` 时默认走它。

**C · 一次性 / 后处理**：

| shader | 自定义 uniform | 入口 |
|--|--|--|
| `vortex` | `vortex_amt = G.TIMERS.REAL - G.vortex_time` | `draw_shader` 单独分支 (`sprite.lua:104-105`) |
| `flame` | `time, amount, texture_details, image_details, colour_1/2, id` | particles + `_send` 自定义 |
| `flash` | `time, mid_flash` | 一次性闪屏 |
| `gold_seal` | `vec4 gold_seal` | 金封蜡 |
| `skew` | 仅 `mouse_screen_pos / hovering / screen_scale` | UI hover 倾斜 |
| `splash` | `time, vort_speed, colour_1/2, mid_flash, vort_offset` | 标题 splash 动画 |
| `background` | `time, spin_time, colour_1/2/3, contrast, spin_amount` | 背景 |
| `CRT` | `distortion_fac, scale_fac, feather_fac, noise_fac, bloom_fac, crt_intensity, glitch_intensity, scanlines, ...` | 后处理（`game.lua:8562-8584` 直接 setShader）|

C 类不一定走 `draw_shader`：`CRT` 在 `Game:draw()` 里直接对整个 canvas
setShader 一次画屏；`background` 由 `BackgroundObject` 自定义 draw；
`flash/splash` 走 `_send` 自定义 uniform 路径。

### 6.1 自定义 uniform 路径（`sprite.lua:98-103`）

`custom_shader == true` 时（即 `_send` 非空），跳过共享 uniform，按列表
一个个 send：

```lua
for k, v in ipairs(_send) do
  G.SHADERS[_shader]:send(v.name,
    v.val or (v.func and v.func()) or v.ref_table[v.ref_value])
end
```

`ref_table[ref_value]` 模式让 lua 端持续修改某个表字段，shader 每帧自动
拉到最新值——**类似 C++ 的指针绑定**。

## Port checklist

| Lua | 移植 | C++ 端 |
|--|--|--|
| `Sprite.atlas / scale / sprite_pos / sprite (Quad)` | keep | `Texture2D` + `Rectangle` 切片；px/py 当成 atlas 元数据 |
| `Sprite:set_sprite_pos` quad 重建 | keep | 改 `Rectangle` 即可，无需重建对象 |
| `Sprite.scale_mag` | keep | 同 |
| `sprite_pos.v` 随机帧 | defer | 动画 atlas 实现时再做 |
| `Sprite.image_dims` | keep | shader uniform 用 |
| `Sprite:draw_self` | keep | 用 `DrawTexturePro(atlas, srcRect, dstRect, origin, r, WHITE)` |
| `prep_draw` 的 push/scale/translate/rotate 链 | keep | 直接搬到 C++ helper；`G.TILESCALE * G.TILESIZE` 当一组常量 |
| `Sprite:draw_from`（借 transform）| defer | 卡背反面 / 复制贴图才用，MVP 不需要 |
| `Sprite.draw_steps` 多 pass 调度 | keep | `std::vector<DrawStep>`，按顺序 BeginShader → Draw → EndShader |
| `define_draw_steps` 接口 | keep | 同字段直译 |
| `Sprite:draw_shader` 共享 uniform 上传 | **keep**（公式逐字抄）| 一个 `UploadStdUniforms(shader, ctx)` 函数 |
| `time = 123.33412 * (ID/1.14212) % 3000` | **keep** | 公式不变；`ID` 用 entt::entity 的 uint 也能跑 |
| `texture_details = (sprite_pos.x, sprite_pos.y, atlas.px, atlas.py)` | keep | vec4 直译 |
| `screen_scale` 公式 | keep | `TILESCALE*TILESIZE*mouse_damping*CANV_SCALE` |
| Shadow 偏移：`VT.{x,y} -= shadow_parrallax * h; VT.scale *= (1-0.2*h)` | keep | 临时改 transform 画完恢复 |
| `_send` 自定义 uniform 路径（ref_table/func）| rewrite | C++ 端用 `std::function` 或直接 lambda；不抄 lua 表反射 |
| `vortex` 单独分支 | keep | `if (shader == "vortex") UploadVortex();` 直译 |
| 10 个卡牌特效 shader（foil/holo/...）| keep | GLSL 330 重写，逐个适配（Phase 2 先选 1 个验证）|
| `dissolve.fs`（默认 fallback） | **keep** | 必须先做，所有共享 uniform 都靠它验证 |
| `CRT.fs` 后处理 | defer | Phase 7 之后；MVP 没屏幕扫描 |
| `background.fs` | defer | 静态背景先用 raylib 自带 ClearBackground |
| `flame / splash / flash / gold_seal / skew` | defer | 各自非核心，需要时再补 |
| `G.SHADERS = {}` 扫目录加载 | rewrite | C++ 端用 `std::unordered_map<string, Shader>` + `LoadShader` |
| `atlas` 表（`game.lua:5660+`）13 个条目 | rewrite | JSON 配置或 `constexpr` 表，path/px/py 三列 |
| `animation_atli`（blind_chips / shop_sign）| defer | 动画系统没起之前不做 |
| `G.I.SPRITE` 全局表 | rewrite | entt view（同 01 笔记）|
| `Sprite.video` 视频纹理 | drop | raylib 没原生视频，也不影响玩法 |
