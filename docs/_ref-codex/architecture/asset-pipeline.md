Source: ref-balatro/resources/{textures,shaders,sounds,fonts}
Last reviewed: 2026-05-01

# Asset pipeline

目标：从 `ref-balatro/resources/` 同步 MVP 子集到 `assets/balatro/`。

## 脚本

`tools/update-balatro-assets.sh`

建议参数：

- `--dry-run`
- `--textures|--shaders|--sounds|--fonts`

## MVP 子集

- textures: `8BitDeck/Jokers/Tarots/Vouchers/...`
- shaders: `holo/foil/polychrome/dissolve/CRT/...`
- fonts: `m6x11plus.ttf`
- sounds: 最小交互音效集

## 提交策略

- `assets/balatro/` 不入库
- 资源通过脚本重建
- 仓库只提交脚本和清单

## Port checklist

- keep: 四类目录结构
- rewrite: Love2D 资源访问到 raylib 路径
- defer: 全量资源校验机制