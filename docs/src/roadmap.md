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

- [x] `src/game/card.h/.cpp` (Card 字段框架)
- [x] `src/game/cardarea.h/.cpp`，type "hand" / "play" 公式
- [x] 8 张卡弧形排列 demo，加/减卡平滑重排
- [x] `engine::AtlasRegistry`（by-name lookup + 跨 tier reload pointer-stable）
- [x] hover/click highlight + 旋转矩形 hit-test（hover 换 cursor，click 切 highlighted lift +40 px）
- [x] drag-to-reorder（按住卡跟手 + 跨邻居 stable_sort by T.x → 邻居 ease 让位 + 释放回 slot）
- [x] `engine::tuning` + ImGui Settings 面板（live tune ease/hand 常数，详见 `project/tuning.md`）

### 经验教训

- **不存 slot 数组、每帧算公式**：第一感是 `array<Vector2, 8>` 存
  死槽位，但 n 变了所有槽位定义都跟着变，状态翻倍。lua 的解法直接
  跑一个 lerp 出 (x, y, r) 写到 `card.T`——Movable ease 自然把 VT
  拉过去，**插值是免费的**。这是 Phase 3 那套 T/VT 分离的真红利：
  排版逻辑可以是纯函数，每帧重算不存中间态。
- **y-bow 公式从 game-unit 翻 pixel-unit 的判断点**：lua 的
  `+ abs - 0.2` 在 G.TILESIZE=32 下整段 y bow 范围 ~0.19 game unit
  ≈ 3 px——肉眼几乎看不见，扇形主要靠旋转。我们 T 直接是像素，照
  抄就 3 px 弧高扁得没扇形味。`bow * card_h * 0.4` 是有意识的放大
  ——是常数 taste 改造，不是 verbatim port。Caveats 里记下来，将
  来真接了 G.TILESCALE 那一层再回头核对。
- **spawn-from-right 比 (0,0) 自然**：新加的卡构造在 `(0, 0, w, h)`
  时，VT 从 viewport 左上角起 ease 几百像素，肉眼上是慢吞吞从左上
  飞下来。改 spawn 落在 hand 右边缘，等于"从 deck 发牌"的视觉
  ——距离短、方向对、不需要动 ease 常数。
- **HardSetCards 解决"启动全飞一遍"**：初始 5 张卡 emplace 后调
  一次 `HardSetCards(0)` 直接 snap 到 slot——开场 = 已发完的状态。
  没这一步的话开场会看到 5 张卡同时从右边滑入，跟"已经在玩"的语
  境矛盾。lua 的 `hard_set_cards` 也是干这个用的。
- **`temp_limit` 的居中收缩效果**：n < temp_limit 时 lerp 公式里
  的 `-0.5*(n-M)/(M-1)` 修正项把整组居中，cards 从两端往中间收。
  要是没这一项，减卡时剩下的全往左堆——丑，且与 lua 行为不符。
- **CardArea 暂不继承 Movable**：MVP 内 area 自己不动（不会缩、
  不会 juice），少一层。Phase 6+ 真要"area 抖一下"再 promote
  ——promote 时 `x_/y_/w_/h_` 退化成 `T()` 的别名包装，不会动
  AlignCards 公式本身。
- **`std::vector<unique_ptr<Card>>` 自有 vs 全局池**：lua 把
  cards 放全局 `G.I.CARD`，CardArea 持非拥有引用（因为卡可以从
  deck 转 hand 转 play）。MVP 没跨 area 转移，自有更简单。`RemoveBack()`
  返 `unique_ptr` 就是给"将来转 area"留的接口——挪到目标 area
  Emplace 即可，不需要重写所有权。
- **AtlasRegistry pointer stability 靠 unordered_map node 不动**：
  reload 用 `it->second = std::move(new_atlas)` 不 `clear()`，
  unordered_map 的 node 是堆上独立分配，rehash 也只动桶不动 node
  地址——`Sprite` / `Card` 持的 `const Atlas*` 跨 tier 切换仍然
  有效。要换成 `flat_hash_map` 这条不成立，得改设计。
- **ImGui FBO V flip 抵消鼠标 y**：`ImGui::Image` 用
  `(ImVec2(0,1), ImVec2(1,0))` 翻转 raylib FBO 上下颠倒；这一步
  之后**鼠标坐标不需要再翻 y**——raylib RT 用 top-left 原点、
  ImGui 也用 top-left，flip 把两边的差互相抵消。`mouse_in_rt =
  mouse_global - image_min` 不带 y 取负。第一版我多翻了一次结果
  hit-test 全在卡的镜像位置上，调了几分钟才意识到。
- **hit-test 必须用 VT，不是 T**：扇形旋转和 juice 缩放都写在 VT，
  T 是目标值（接近静态）。lua 用 `CT = VT` 同步是出于一样的考虑
  ——拖动中卡的视觉位置在 VT，命中也得在 VT，否则点击会落在卡的
  "目标位置"而不是"现在你看到的位置"，偏几像素就感觉不对。
- **输入门控走 ImGui::IsItemHovered 而非 raylib**：raylib
  `IsMouseButtonPressed` 不知道菜单栏 / Themes 面板的存在，hand
  会接到所有点击。ImGui 的 `IsItemHovered()` + `IsMouseClicked()`
  跟 ImGui 的输入仲裁配合，菜单栏 / 面板上的 click 不会穿透。这
  也意味着 input 必须挂在 OnImGuiRender 里、而不是 OnUpdate
  ——多 1 帧延迟，肉眼无感。
- **drag 跟手必须 snap VT.x/y**：写完 `dragged.T = mouse - offset`
  之后 Movable 的 ease 还在跑，VT 滞后 T 几十 ms——drag 时这种
  滞后等于卡在跟着鼠标"游泳"。手动 `dragged.VT() = T` 强制无延
  迟。r/scale 不 snap 还是好的：抓起卡时 fan tilt 平下来 + juice
  余响要顺出来。第一版按 lua "verbatim 抄" 没踩到，drag 接进来才
  暴露。
- **lua taste 常数搬 pixel-direct 出 BUG**：`max_vel = 70` 在 lua
  是 game-units/sec（× tile-scale 约 30 → 2100 px/sec），我们当
  pixel/sec 直接搬就是 70 px/sec ≈ 1.17 px/frame@60fps，drag 200
  px snapback 要 2.8 秒。`sway_coeff = 0.015` 同样问题（vel.x 量
  级差 30 倍 → 卡飞着打转）。教训：lua 的 `verbatim 抄 K 常数`
  这条规则有个隐藏前提是 game-unit 量级。我们的 pixel-direct port
  里全部得按 ~30× 的换算系数核对，或者干脆暴露成 tunable（最后选
  的就是后者，加了 `engine::tuning` + Settings 面板）。
- **drag 无 z-order 必须 Render 时单独画 dragged**：vector 顺序
  = 画顺序，stable_sort 把 dragged 挪到中间索引时，会被右边邻居
  画上来盖住。lua 的 hover/drag 都提 z；我们 MVP 的简化做法：
  Render 里 if-skip dragged + 末尾再画一次。zero-alloc，零 z 状
  态机。

## Phase 6 — 一轮玩法循环

- [ ] 状态机 DRAW_TO_HAND → SELECTING_HAND → PLAY_HAND → PLAYING_HAND → ROUND_FINISHED
- [ ] 出牌 → 扑克手牌识别 → chips × mult 记分
- [ ] HUD 自绘 (DrawTextEx) 显示分数/目标分/手数/弃牌数

### 经验教训
_(待填)_

## Phase 7 — Joker / Planet / Tarot + Shader 起点

留作未来 plan，本阶段不展开。
