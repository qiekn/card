# Balatro-Like 卡牌游戏：研究 + MVP 路线图

Context

用户想从零仿做一个 Balatro 类型的 2D 卡牌游戏。背景信息：
- 用户 CG 基础扎实（OpenGL/Vulkan/raylib），熟悉 C++，但没写过任何卡牌游戏。
- 复用 Balatro 的美术资产和音频（ref-balatro/resources/，已被 .gitignore）。
- MVP 范围：先不做商店、关卡、Boss Blind，只做一轮"指定目标分数 → 出牌得分"的最小可玩闭环，含：手牌区 / 出牌区 / 小丑牌区 / 星球牌区
/ 塔罗牌区，目标分等信息走 ImGui 面板。
- 后续要做：贴花、印章、卡牌 shader 链。
- 4K 显示器（DPI 缩放 + 高分辨率素材重采样要早一点定）。
- 当前项目状态：raylib 6.0 + Dear ImGui (docking) + raygui submodules 已就位，layered 架构（GameLayer + ImGuiLayer）+ FBO viewport 已
working。
- 用户希望"一点点分析"——先研究 Balatro 怎么做的，做笔记，再动手写代码。这份 plan 不直接写代码，只规划研究 + MVP 框架。

用户决策（已确认）

┌─────────────┬─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│    议题     │                                                        选择                                                         │
├─────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ 第一步策略  │ 先把研究笔记写完，再开 src/。Phase 1 是纯研究 + 文档输出，没有 C++ 改动。                                           │
├─────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Balatro     │ 拷贝到 assets/balatro/。写 tools/update-balatro-assets.sh 把 textures/shaders/sounds/fonts 子集从                   │
│ 资产复用    │ ref-balatro/resources/ 拷过来；.gitignore 排除 assets/balatro/ 避免提交受版权资产。                                 │
├─────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ 游戏内 HUD  │ 游戏内自绘：ImGui 只做开发者面板（Inspector/Themes/Console，可整体隐藏）；目标分、手数、弃牌数、牌堆数量等游戏内    │
│ 风格        │ HUD 用 raygui + DrawTextEx 在 FBO 内绘制——更贴近 Balatro 视觉语言。                                                 │
└─────────────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

研究阶段已完成的初读（这份 plan 一并归档）

读过的核心文件（路径相对 ref-balatro/）：
- engine/object.lua (~37 行) — SNKRX 风格的极简 OOP 基类
- engine/node.lua (~400 行) — 带 transform / container / children / hit-test 的场景节点
- engine/moveable.lua (~597 行) — 核心创新：T/VT 双 transform + Major/Minor role 系统
- engine/sprite.lua (~262 行) — atlas + quad 切片 + draw_steps shader 链
- engine/particles.lua (~208 行) — emit-on-timer 的粒子系统
- engine/ui.lua (前 200 行) — UIBox 声明式 UI 树（仅了解，不移植）
- cardarea.lua (~1024 行) — 容器，按 type ("deck"/"hand"/"play"/"joker"/"consumeable"/"shop"/...) 用不同公式排列卡牌
- card.lua (前 120 行) — 单卡，children = {shadow, front, back, center}
- main.lua 入口 + game.lua:8328-8470 的 Game:draw() — 渲染顺序

三个关键发现（要写进笔记 + 后面移植时反复参考）

1. T/VT 双 transform 是 Balatro 流畅手感的灵魂。业务代码只设置 T.x = 5，VT.x 每帧用指数 ease 平滑追上：
self.velocity.x = G.exp_times.xy * self.velocity.x
               + (1 - G.exp_times.xy) * (self.T.x - self.VT.x) * 35 * dt
self.VT.x = self.VT.x + self.velocity.x
1. align_cards() 每帧重写所有卡的 T 也不卡，就是因为 VT 在追。
2. CardArea 的 type 决定整套排列公式——没有可插拔的 layout 策略对象，就是 if-else by config.type。手牌弧形排在
cardarea.lua:692-722，出牌区直线排在 cardarea.lua:787-810。
3. 卡牌的 edition / seal / sticker / holographic 是 Sprite::draw_steps 的多 shader pass，不是单独的实体。每张卡画一次 = 多个 shader
叠加。shader 文件在 ref-balatro/resources/shaders/*.fs (GLSL 1.20，需轻调成 raylib 330)。

完整三个发现的扩展、代码引用、设计决策——都进 docs/。

路线图

Phase 1 — 研究笔记 (估计 2-3 天，纯文档，无 C++ 改动)

目标：把 ref-balatro 里跟我们 MVP 相关的子系统全部精读 + 输出 .md 笔记。这一阶段不写 src/ 代码。

新建 docs/ 目录，初始结构：
docs/
├── README.md                       # 这个 docs/ 怎么用 + 索引
├── balatro/                        # ref-balatro 源码逐子系统精读
│   ├── 01-object-node-moveable.md  # T/VT、role、juice、indexing 集合
│   ├── 02-cardarea-card.md         # 容器/排列公式/select/highlight/play 流程
│   ├── 03-sprite-shader.md         # atlas + quad、draw_steps、uniforms 列表
│   ├── 04-particles.md             # emit / lifetime / attach
│   ├── 05-ui-system.md             # 仅作了解，不移植；解释为什么不移植
│   └── 06-draw-pipeline.md         # G.I.* 集合、Game:draw 顺序、CANVAS
├── architecture/                   # 我们的 C++ 设计决策
│   ├── port-decisions.md           # 哪些原样移植 / 哪些重写 / 哪些丢
│   ├── ecs-vs-oo.md                # entt 怎么承接 Node/Movable
│   ├── shader-uniforms.md          # 我们 raylib 端的 uniform 命名 + 类型对照表
│   ├── hud-rendering.md            # 自绘 HUD 在 FBO 内的层次
│   └── asset-pipeline.md           # update-balatro-assets.sh 怎么用 + 路径约定
└── roadmap.md                      # 实现顺序 + 当前进度（每个 Phase 完成后勾选）

每篇笔记的写作要求：
- 顶部 frontmatter：source: ref-balatro/<file>:<line-range> + last reviewed: YYYY-MM-DD
- 不超过 300 行，超过就拆
- 引用 ≤4 行的 Lua 片段，不要大段拷贝
- 末尾"port checklist"小节 — 列出移植到 C++ 时要保留 / 改名 / 丢弃的字段

每篇完成后追加到 docs/roadmap.md 的 "Phase 1 progress" 列表。

Phase 1 出口（gate）

- 6 篇 docs/balatro/*.md + 5 篇 docs/architecture/*.md 全部存在
- docs/architecture/port-decisions.md 的"原样移植 / 重写 / 丢"三栏完整 (~30 个条目)
- docs/architecture/shader-uniforms.md 列出所有 Balatro shader 的 send 调用
(mouse_screen_pos/screen_scale/hovering/dissolve/...)，以及 raylib 端 SetShaderValue 的对应 Vector4/Float/Vector2 类型

Phase 2 — 工程基础设施 (1 天)

只有 Phase 1 出口达成才开始。

- tools/update-balatro-assets.sh：把 ref-balatro/resources/{textures,shaders,sounds,fonts} 里需要的子集拷贝到
assets/balatro/。脚本支持 --dry-run 和按子目录拷贝。
- .gitignore：加 /assets/balatro/（资产受版权，不进仓库；新人 clone 后跑一次脚本生成）
- README.md：加一节"Notes & design docs"——
▎ Notes and design decisions live in docs/. Always update the relevant note when changing or learning a system; treat docs/ as the
second source of truth alongside the code.

- 再加一节"First-time asset setup"说明跑 tools/update-balatro-assets.sh。
- CMakeLists.txt：把 assets/balatro/ 加进 install/copy 列表（如果将来需要）。
- shader 适配：选 1 个 shader（推荐 holo.fs）做 GLSL 1.20→330 适配实验，把过程写进 docs/architecture/shader-uniforms.md。

Phase 3 — 引擎抽象 (1-2 天)

C++ 类草图（先在 docs/architecture/port-decisions.md 定稿，再写 src/engine/）：
// src/engine/transform.h
struct Transform { float x, y, w, h, r, scale; };

// src/engine/movable.h — 对应 Lua Moveable 的 T/VT 部分
class Movable {
public:
   Transform T;        // target
   Transform VT;       // visible (eased)
   Vector2  velocity{};
   void Move(float dt);          // 每帧调用，用指数 ease 把 VT 追到 T
   void HardSetT(...);           // 瞬时同步 VT = T (放卡时用)
   void JuiceUp(float a = 0.4f); // squash & stretch 弹一下
};
先不做 Major/Minor role 系统——先让每个 Movable 各自移动；CardArea + Card 一旦跑起来再判断要不要把 Major/Minor 加上。这个简化决策记入
port-decisions.md。

Phase 4 — Sprite + 卡背 + 一张卡 (1-2 天)

- 把 assets/balatro/textures/cards_1.png 等 atlas 加载进来
- 实现 class Sprite : public Movable，支持 quad slicing（参考 engine/sprite.lua:33-39）
- 在 viewport 中央画一张静态卡，验证 atlas + transform + ease 通了

Phase 5 — CardArea + 手牌弧形排列 (2-3 天)

- 实现 class CardArea : public Movable 持有 std::vector<Card*>
- 优先移植 cardarea.lua:692-722 的 hand 弧形公式和 cardarea.lua:787-810 的 play 直线公式
- 测试：键盘加/减卡数量，卡牌平滑滑动到新位置

Phase 6 — 一轮玩法循环 (3-5 天)

游戏状态机最小集：
DRAW_TO_HAND → SELECTING_HAND → PLAY_HAND →
 PLAYING_HAND (打分动画) → ROUND_FINISHED (赢/输判定)
- 牌堆 / 弃牌堆只做后端 std::vector，不画
- 出牌 = 算扑克手牌等级 (高牌/对子/...) → 按 chips × mult 公式记分
- HUD 自绘：分数 / 目标分 / 出牌次数 / 弃牌次数用 DrawTextEx 直接画在 FBO 内固定位置（参考 Balatro 截图布局）；只有 Inspector /
Themes / 调试值用 ImGui

Phase 7 — 小丑/星球/塔罗占位 + Shader 起点 (这之后再讨论)

留作未来 plan，本 plan 不展开。

关键代码 / 资源引用

实现时直接引用的 Lua 行号（写笔记时也要标记）：
- cardarea.lua:692-722 — 手牌弧形排列公式
- cardarea.lua:787-810 — 出牌区直线排列
- cardarea.lua:811-852 — joker / consumeable 排列
- moveable.lua:453-480 — move_xy() 指数 ease 公式（直接抄）
- moveable.lua:268-300 — juice_up() + move_juice() squash 动画
- sprite.lua:75-156 — draw_shader() uniform 送参顺序
- game.lua:8328-8470 — Game:draw() 渲染顺序

资产路径（拷过来后变成 assets/balatro/...）：
- 卡牌图集：textures/cards_1.png, textures/Joker.png, textures/Tarot.png, textures/Planet.png, textures/Voucher.png,
textures/boosters.png
- shader：shaders/holo.fs, foil.fs, polychrome.fs, negative.fs, dissolve.fs, flash.fs, flame.fs, gold_seal.fs, debuff.fs, booster.fs,
background.fs, CRT.fs
- 字体：fonts/m6x11plus.ttf
- 音效：sounds/*.ogg (按需)

Verification

每个 Phase 完成后的验证标准：

- Phase 1 (笔记)：6 + 5 篇 .md 全部存在；随手翻一篇能 5 分钟读懂；port-decisions.md 的三栏完整。
- Phase 2 (基建)：删掉 assets/balatro/ 后跑 tools/update-balatro-assets.sh 能恢复；至少 1 个 shader 能在 raylib 里编译通过。
- Phase 3 (引擎)：键盘 1/2/3 切换 Movable.T.x，目视确认 VT.x 平滑跟随，松手后 0.3s 内停住，无抖动。
- Phase 4 (Sprite)：viewport 中央显示一张 Joker 卡背，可以缩放/旋转，atlas quad 边界正确。
- Phase 5 (CardArea)：屏幕底部弧形排开 8 张卡；按 D 减一张时其它卡平滑重排。
- Phase 6 (玩法)：能从默认 52 张牌堆抽 8 张到手牌，鼠标点击高亮，按 Play 把高亮卡推到出牌区，HUD 文字显示得分；分数到达目标进入
ROUND_FINISHED。

每个 Phase 完成后必须更新 docs/roadmap.md（勾选 + 写一段经验教训），否则该 Phase 不算完。
