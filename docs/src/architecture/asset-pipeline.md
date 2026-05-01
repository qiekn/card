---
source: Phase 2 工程基础设施——asset 同步流程
---

# Asset Pipeline

> Balatro 的资产受版权保护，**不能入仓**。每个开发者本地从自己买的 Steam
> 装机里抽取一次，未来更新游戏时再跑一次脚本即可。

## 1 · ref-balatro 资产清单

`ref-balatro/resources/` 实际目录：

```text
resources/
├── fonts/              # 9 个 .ttf（含 m6x11plus + GoNoto/NotoSans CJK）
├── shaders/            # 19 个 .fs（LÖVE GLSL 110）
├── sounds/             # 80 个 .ogg
├── textures/
│   ├── 1x/             # 13 个 atlas + collabs/ 子目录
│   ├── 2x/             # 同上，2 倍分辨率
│   └── 4x/             # 同上，4 倍分辨率（仅 Steam 完整版有）
└── gamecontrollerdb.txt
```

我们项目要用的：

| 子目录 | 处理方式 |
|--|--|
| `textures/{1x,2x,4x}/*.png` | **直接拷贝** 到 `assets/balatro/textures/` |
| `fonts/*.ttf` | **直接拷贝** 到 `assets/balatro/fonts/` |
| `sounds/*.ogg` | **直接拷贝** 到 `assets/balatro/sounds/` |
| `shaders/*.fs` | **手工 port 到 GLSL 330** 后存到 `assets/shaders/`（不自动同步）|
| `gamecontrollerdb.txt` | 直接拷 |

textures 占大头（1x 大约 50 MB，2x 200 MB，4x 800 MB）。MVP 用 1x 够，
2x/4x defer 到 Phase 7 配置项。

## 2 · 来源：Steam 装机

Balatro 用 [LÖVE 2D fused executable](https://love2d.org/wiki/Game_Distribution)
打包，**`Balatro.exe` 本质是带 LÖVE runtime 头的 zip**——直接用 7z / unzip
就能解。

Steam 默认安装路径：

| OS | 路径 |
|--|--|
| Windows | `C:/Program Files (x86)/Steam/steamapps/common/Balatro/Balatro.exe` |
| macOS | `~/Library/Application Support/Steam/steamapps/common/Balatro/Balatro.app/Contents/Resources/Balatro.love` |
| Linux | `~/.steam/steam/steamapps/common/Balatro/Balatro.exe` |

非 Steam 用户（GOG / Epic）等可以用环境变量 `BALATRO_PATH` 覆盖。

## 3 · `tools/update-balatro-assets.sh`

工程基础设施 Phase 2 第一项任务。脚本责任：

1. **定位** Balatro 安装（Windows / macOS / Linux 三档 + `BALATRO_PATH`
   env override）
2. **校验**版本号（`metadata/version.jkr` 或类似——避免拷错版本）
3. **解压**到临时目录（`mktemp -d` + `unzip -q`）
4. **复制** 4 类资产到 `assets/balatro/`：
   - `textures/1x/` →（必需）
   - `textures/2x/` →（可选 flag `--with-2x`）
   - `textures/4x/` →（可选 flag `--with-4x`）
   - `fonts/`、`sounds/`、`gamecontrollerdb.txt` 一次性全拷
5. **不复制** `shaders/`（提醒：手工 port，见 architecture/shader-uniforms.md）
6. **dry-run 模式** `--dry-run`：只列出会复制的文件，不真动盘

脚本要 idempotent：重跑覆盖已有文件，不删除 `assets/balatro/` 之外的内容。

### 3.1 验收

Phase 2 完成 gate（来自 roadmap.md）：

- `tools/update-balatro-assets.sh --dry-run` 能正确列出 ≥ 50 MB 文件
- 实跑后 `assets/balatro/textures/1x/8BitDeck.png` 存在且 71×95×52
- `.gitignore` 含 `/assets/balatro/`
- README 有 "First-time asset setup" 段教用户跑一次

## 4 · `.gitignore`

```text
/assets/balatro/         # Phase 2 加：Balatro 原作资产，本地同步
/assets/cache/           # 后期可能加：转码 / 压缩缓存
```

`assets/shaders/`（我们手 port 的 GLSL 330）**入仓**——这是我们的成果。

## 5 · 启动时检查

C++ 端 `Game::Init()` 第一件事是 `AssertAssetsPresent()`：

- 检查关键路径 `assets/balatro/textures/1x/8BitDeck.png` 是否存在
- 不存在则 `fprintf(stderr, ...)` 输出**清晰指引**：
  「跑 `tools/update-balatro-assets.sh` 同步 Balatro 资产；详见 README」
- 然后 `exit(1)`，不继续

不要静默失败——首次构建后启动崩溃的用户体验比"提示去跑脚本"差得多。

## 6 · Atlas 元数据

15 个 atlas 的 `(name, path, px, py)` 在 `game.lua:5660-5735`（03 笔记
§2.2 列出）。这些数据**不来自资产文件本身**——Balatro 是把元数据硬编码在
代码里。

我们项目的 `assets/atlases.json`（或 `.toml`）需要**手抄**这 15 项：

```text
[
  { "name": "cards_1", "path": "8BitDeck.png",      "px": 71,  "py": 95 },
  { "name": "cards_2", "path": "8BitDeck_opt2.png", "px": 71,  "py": 95 },
  { "name": "centers", "path": "Enhancers.png",     "px": 71,  "py": 95 },
  { "name": "Joker",   "path": "Jokers.png",        "px": 71,  "py": 95 },
  { "name": "Tarot",   "path": "Tarots.png",        "px": 71,  "py": 95 },
  { "name": "Voucher", "path": "Vouchers.png",      "px": 71,  "py": 95 },
  { "name": "Booster", "path": "boosters.png",      "px": 71,  "py": 95 },
  { "name": "ui_1",    "path": "ui_assets.png",     "px": 18,  "py": 18 },
  { "name": "ui_2",    "path": "ui_assets_opt2.png","px": 18,  "py": 18 },
  { "name": "balatro", "path": "balatro.png",       "px": 333, "py": 216},
  { "name": "gamepad_ui","path":"gamepad_ui.png",   "px": 32,  "py": 32 },
  { "name": "icons",   "path": "icons.png",         "px": 66,  "py": 66 },
  { "name": "tags",    "path": "tags.png",          "px": 34,  "py": 34 },
  { "name": "stickers","path": "stickers.png",      "px": 71,  "py": 95 },
  { "name": "chips",   "path": "chips.png",         "px": 29,  "py": 29 }
]
```

外加 2 个 animation atlas（`blind_chips` 21 帧、`shop_sign` 4 帧，
`game.lua:5644-5659`）defer。collabs/ 子目录里的 `collab_*` 一对对
跟卡牌同尺寸（71×95），MVP 不接 IP 联动 —— 也 defer。

**这份 JSON 入仓**——它是我们对 Balatro 资产的"接口定义"，不依赖资产
本身。**注意位置**：放在 `assets/atlases.json`（顶层），不放 `assets/balatro/`
里——后者被 `.gitignore`，手抄元数据要避开。

## 7 · Sprite 网格映射（`G.P_CENTERS` 等）

每个 atlas 上的"哪格是哪张卡"也是硬编码的 lua 表（`functions/Card_Tables.lua`
~1000 行）。这是 Phase 6 的工作，不在 asset pipeline 范围。

对应表大致结构：

```text
G.P_CENTERS = {
  c_red_seal = { name=..., pos={x=0, y=4}, atlas="centers", ... },
  m_gold     = { name=..., pos={x=8, y=0}, atlas="centers", ... },
  j_joker    = { name="Joker", pos={x=0, y=0}, atlas="Joker", ... },
  ...
}
```

Phase 6 时把 `Card_Tables.lua` 解析成 `assets/cards.json` 入仓
（跟 `atlases.json` 同位置，避开 gitignored `assets/balatro/`）。

## 8 · 不做的事

- **不重新生成 atlas**——直接用原图，避免重打包丢精度 / 错位
- **不压缩纹理**（Phase 7 才考虑 PVRTC / BCn）
- **不混入新美术资源**——Phase 9+ 要做的话再开 `assets/custom/` 子目录
- **不自动同步 shader**——必须人工 review 每个 `.fs` 的 GLSL 330 移植结果
- **不下载安装 Balatro**——脚本只检测已装的，不替用户买游戏

## 9 · Phase 顺序

| Phase | asset 相关任务 |
|--|--|
| 2 | 写 `update-balatro-assets.sh` + `.gitignore` 加 `/assets/balatro/` + README 加 setup 段 + `atlases.json` 写完 |
| 2 | 选 1 个 shader（推荐 `dissolve.fs`）port 到 GLSL 330，写到 `assets/shaders/` |
| 3 | 启动时 `AssertAssetsPresent()` |
| 4 | 加载 1 个 atlas（推荐 `Joker.png`）+ 显示 1 张卡 |
| 4 | port 第二个 shader（推荐 `holo.fs`） |
| 6 | 解析 `Card_Tables.lua` → `cards.json` |
| 7+ | port 余下 8 个卡牌特效 shader |
| 7+ | 加 2x atlas 选项（settings 切换） |
| 8+ | 4x atlas + 视频素材（如保留）|

`update-balatro-assets.sh` 的写法是 Phase 2 实施时定，本篇只锁定**架构
形式**：单脚本 + 入仓 atlases.json + 启动校验 + 不动 shader 自动同步。
