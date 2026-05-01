---
source: Phase 2 工程基础设施——asset 同步流程
---

# Asset Pipeline

> **策略**：本仓库为个人学习项目，不商用、不公开发布。MVP 真正用到的
> Balatro 1x / 2x 贴图直接 vendor 到 `assets/textures/{1x,2x}/` 入仓，
> 这样 clone 就能跑、视频演示也方便。`tools/update-balatro-assets.sh`
> 仍保留——它把 Steam 装机里的资产抽到本地 staging dir
> `assets/balatro/`（gitignored），方便 Balatro 版本更新后用 `diff`
> 决定哪些贴图需要重新 vendor。

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
| `textures/{1x,2x}/*.png` | **vendor 入仓** 到 `assets/textures/{1x,2x}/`（MVP 实际只用到 ~15 个 atlas，先全拷过来省事）|
| `textures/4x/*.png` | 不入仓（800 MB 太大，1080p 显示也看不出差别）|
| `fonts/*.ttf` | 暂不导入（保留我们自己的 OpenSans，本地化阶段再换 m6x11plus + GoNoto CJK）|
| `sounds/*.ogg` | 暂不导入，Phase 7 加音频时再决定 |
| `shaders/*.fs` | **手工 port 到 GLSL 330** 后存到 `assets/shaders/`（不自动同步）|
| `gamecontrollerdb.txt` | 暂不导入，没接 gamepad |

textures 1x ≈ 50 MB、2x ≈ 200 MB——两份都入仓让 Themes 面板的
"Texture Scale" combo 能在运行时切（见 `project/sprite.md` Caveats）。
4x defer 到不大可能的将来。

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

把 Steam 装机里的资产抽到本地 staging dir `assets/balatro/`（gitignored），
方便 Balatro 版本更新后用 `diff` 决定哪些贴图需要 re-vendor。脚本责任：

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

vendor 流程（手动，不自动化——希望开发者每次都看一眼 diff）：
`assets/balatro/textures/1x/*.png` → 用得到的拷到 `assets/textures/1x/`，
2x 同理。当前 MVP 把 1x、2x 整目录都 vendor 了，将来文件多了再筛。

### 3.1 验收

Phase 2 完成 gate（来自 roadmap.md，已达成）：

- `tools/update-balatro-assets.sh --dry-run` 能正确列出 ≥ 50 MB 文件
- 实跑后 `assets/balatro/textures/1x/8BitDeck.png` 存在且 71×95×52
- `.gitignore` 含 `/assets/balatro/`
- README 有 "First-time asset setup" 段（仅当你想 re-sync 时跑一次）

## 4 · `.gitignore`

```text
/assets/balatro/         # 脚本 staging dir：Balatro 原作资产，本地同步
/assets/cache/           # 后期可能加：转码 / 压缩缓存
```

入仓的 asset 目录：

- `assets/textures/{1x,2x}/`——MVP 用到的 Balatro 贴图（学习用途）
- `assets/atlases.json`——15 个 atlas 的 (name, path, px, py) 元数据
- `assets/themes.json`——ImGui 主题
- `assets/icons/`、`assets/fonts/opensans/`——我们自己的 UI 资源
- `assets/shaders/`——手工 port 的 GLSL 330（这是我们的成果）

## 5 · 启动时检查

设计上原本要在 `Game::Init()` 第一件事跑 `AssertAssetsPresent()`：检查
关键路径，缺失就 stderr 提示 + `exit(1)`。**当前实现 defer**——
`engine::Atlas` 构造函数只 `fprintf(stderr, ...)` 一行警告 + 留空
texture（`id == 0` 哨兵），调用方 `if (!atlas.Loaded()) return;` 跳过
渲染，不 crash。

这是 policy debt：MVP 阶段方便迭代，后期文件多了要补回 fail-fast 校验，
否则缺一张图只会"该卡牌不画"，靜默失败比崩溃难调试得多。Phase 6
（解析 `cards.json`）之前补。

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

**这份 JSON 已入仓**（`assets/atlases.json`）——它是我们对 Balatro 资产
的"接口定义"，不依赖资产本身。注意 `px` / `py` 是 1x baseline，loader
（`engine::Atlas`）按当前 texture scale 乘以 tier 得到实际 cell 大小。

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
（跟 `atlases.json` 同位置）。

## 8 · 不做的事

- **不重新生成 atlas**——直接用原图，避免重打包丢精度 / 错位
- **不压缩纹理**（Phase 7 才考虑 PVRTC / BCn）
- **不混入新美术资源**——Phase 9+ 要做的话再开 `assets/custom/` 子目录
- **不自动同步 shader**——必须人工 review 每个 `.fs` 的 GLSL 330 移植结果
- **不下载安装 Balatro**——脚本只检测已装的，不替用户买游戏

## 9 · Phase 顺序

| Phase | asset 相关任务 | 状态 |
|--|--|--|
| 2 | 写 `update-balatro-assets.sh` + `.gitignore` 加 `/assets/balatro/` + `atlases.json` 写完 | done |
| 2 | 选 1 个 shader（推荐 `dissolve.fs`）port 到 GLSL 330，写到 `assets/shaders/` | defer |
| 3 | 启动时 `AssertAssetsPresent()` | defer（Atlas ctor fprintf+noop 兜底）|
| 4 | 加载 1 个 atlas（`Jokers.png`）+ 显示 1 张卡 | done |
| 4 | port 第二个 shader（`holo.fs`） | defer |
| 4 | vendor 1x + 2x textures + Themes 面板 "Texture Scale" combo | done |
| 6 | 解析 `Card_Tables.lua` → `cards.json` | todo |
| 7+ | port 余下 8 个卡牌特效 shader | todo |
| 8+ | 4x atlas + 视频素材（如保留）| 不计划 |

`update-balatro-assets.sh` 的写法是 Phase 2 实施时定，本篇只锁定**架构
形式**：单脚本（staging）+ 手动 vendor + 入仓 atlases.json + 不动 shader
自动同步。
