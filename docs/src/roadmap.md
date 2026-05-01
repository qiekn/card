# Roadmap

记录 MVP 路线图的进度。每个 Phase 完成后勾选 + 写"经验教训"。

完整 plan: `~/.claude/plans/balatro-fluttering-mccarthy.md`。

## Phase 1 — 研究笔记

无 C++ 改动，纯文档。完成的 gate：6 篇 `balatro/*.md` + 5 篇 `architecture/*.md` 全部存在；`port-decisions.md` 三栏完整；`shader-uniforms.md` 覆盖所有 Balatro shader 的 uniform。

- [x] `balatro/01-object-node-moveable.md`
- [x] `balatro/02-cardarea-card.md`
- [x] `balatro/03-sprite-shader.md`
- [x] `balatro/04-particles.md`
- [x] `balatro/05-ui-system.md`
- [x] `balatro/06-draw-pipeline.md`
- [x] `architecture/port-decisions.md`
- [x] `architecture/ecs-vs-oo.md`
- [x] `architecture/shader-uniforms.md`
- [x] `architecture/hud-rendering.md`
- [x] `architecture/asset-pipeline.md`

### 经验教训

_(每勾掉一项就在这里补一段：哪些 lua 行没看懂折腾了多久 / 哪个公式实际意义和我猜的不一样)_

## Phase 2 — 工程基础设施

- [x] `tools/update-balatro-assets.sh` 写完 + 验证 dry-run
- [x] `.gitignore` 加 `/assets/balatro/`
- [x] `README.md` 加 "First-time asset setup" 段
- [x] 1 个 shader（推荐 holo.fs）改成 raylib 330 编译通过

### 经验教训

- `update-balatro-assets.sh`: PowerShell 造的 zip 用 backslash 路径分隔符，
  unzip 退 1（warning）会被 `set -e` 当 fatal 杀掉——只把 exit ≥ 2 当
  真错误。用 `BALATRO_PATH` 指向 `ref-balatro/` 临时打包的 zip 就能本地
  end-to-end dry-run 验证（无须真 Steam 装机）。
- shader port: dissolve 选作头炮的判断对——共享 uniform（dissolve / time
  / texture_details / image_details / shadow / burn_colour_*）和共享 vertex
  hover transform 全部一次走通。当下没装 glslangValidator，编译验证延到
  Phase 3 起 raylib `LoadShader` 时做。`assets/shaders/common.{vs,glsl}`
  抽取留到第二个卡牌特效 shader（holo）port 时一起做，避免单 shader 时
  过早抽象。

## Phase 3 — 引擎抽象 (Movable T/VT)

- [x] `src/engine/transform.h`
- [x] `src/engine/movable.h/.cpp` (Move / HardSetT / JuiceUp)
- [x] 键盘 1/2/3 切换 T.x，VT.x 平滑跟随的 demo

### 经验教训

- **`Transform` 撞 raylib**：raylib 也定义 `Transform`（骨骼蒙皮 4×4 + 四元数，
  `raylib.h:451`）。第一次 include 既有 raylib 又有我们 transform.h 的
  .cpp 立刻 redefinition error。整个 engine/ 进 `engine::` namespace 解决。
  现有 src/{game,layer,*}.cpp 还是全局 namespace —— 风格不一致，但 engine/
  这一层加 namespace 更安全（未来还会引入 `engine::Sprite` / `Camera` 等）。
- **MoveR 必须在 MoveXY 后**：sway 项 `0.015 * vel.x / dt` 读 `velocity_.x`，
  顺序错了 sway 永远 0。lua 顺序照抄即可。
- **MoveScale 必须在 MoveJuice 后**：juice 通过加进 `des_scale` 注入 wobble，
  不直接写 VT.scale ——这样移动中 + juice 不打架。
- **ease 公式的 `(T - VT) * 35 * dt`**：35 跟 50（K_xy）是两个独立的口味
  常数。35 是力的强度（位置误差转 velocity 增量），50 是衰减速率。两个都
  要原样抄，改一个手感就变。
- **snap 阈值 0.01 / 0.001 / 0.001**：浮点不收敛只能 if-snap 强咬死。
  位置阈值粗一档（0.01 game unit ≈ 半像素以下），旋转 / scale 严一档
  （转动 / 缩放更显眼）。

## Phase 4 — Sprite + 一张卡

- [x] `src/engine/sprite.h/.cpp` (atlas + quad slicing)
- [x] viewport 中央渲染一张卡，atlas quad 边界正确

### 经验教训

- **Movable 是 move-only**（`Velocity` / `std::optional<Juice>` 不让拷），
  Sprite 直接继承下来也是 move-only。GameLayer 想"延迟构造"——OnAttach 时
  才 load 完 Atlas 才能造 Sprite——`std::optional<engine::Sprite>` + emplace
  是最干净的写法。指针 / 引用都不行（前者要管寿命、后者不能 reseat）。
- **Atlas 用 raylib `Texture2D.id == 0` 当 sentinel "empty"**：dtor 只在
  `id != 0` 时 UnloadTexture，move ctor / op= 把源 id 清零。POD 配 RAII
  没有真"析构异常"问题，写起来比 unique_ptr<Texture2D, Deleter> 短一截。
- **lua `prep_draw + draw_self` 整条链压成一个 `DrawTexturePro`**：raylib
  这个 API 自带 `origin` 参数，把"绕中心旋转"内化掉，省了手写
  push/translate/rotate/translate/pop 的 OpenGL matrix 链。代价是不能多
  pass（要加 shadow/shader 时得手回退到 `BeginMode2D` + 多次绘）。
- **Atlas 写死 `TEXTURE_FILTER_POINT`**：Balatro 是 pixel art，1px 描边被
  bilinear 糊成 2px 模糊带。所有 atlas 都该 POINT，没构造参数让 caller 选
  ——以后真有非 POINT 需求再加。和 RT 端 POINT filter 是叠加效果：
  atlas POINT → RT POINT → ImGui::Image 缩放（仍是 POINT）。
- **资产缺失走 fprintf + Render noop，不 exit(1)**：跟
  architecture/asset-pipeline.md §5 写的 `AssertAssetsPresent → exit(1)`
  不一致。MVP 选 noop 因为没装 Balatro 还想能跑别的——但这是债，Phase 5
  加 AtlasRegistry 时要统一决策（要么所有 atlas 失败都 exit，要么补
  fallback texture）。

## Phase 5 — CardArea + 手牌弧形

- [ ] `src/game/card.h/.cpp` (Card 字段框架)
- [ ] `src/game/cardarea.h/.cpp`，type "hand" / "play" 公式
- [ ] 8 张卡弧形排列 demo，加/减卡平滑重排

### 经验教训
_(待填)_

## Phase 6 — 一轮玩法循环

- [ ] 状态机 DRAW_TO_HAND → SELECTING_HAND → PLAY_HAND → PLAYING_HAND → ROUND_FINISHED
- [ ] 出牌 → 扑克手牌识别 → chips × mult 记分
- [ ] HUD 自绘 (DrawTextEx) 显示分数/目标分/手数/弃牌数

### 经验教训
_(待填)_

## Phase 7 — Joker / Planet / Tarot + Shader 起点

留作未来 plan，本阶段不展开。
