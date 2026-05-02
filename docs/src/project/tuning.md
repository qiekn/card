---
source: src/engine/tuning.h, src/imgui_layer.cpp::DrawSettingsPanel
---

# Tuning 与 Settings 面板

Phase 5 加进来的内容。手感常数（Movable ease 的衰减率 / 力增益、hand
弧形深度、选中拔高等等）从 `constexpr` 改成了运行时可改的全局，附一个
ImGui Settings 面板做 live tune。

## 1 · 问题：lua 的"taste 常数"搬到 pixel 直接出 BUG

Phase 3 抄 Movable 时记下来"K 常数 50/60/190 是 taste，verbatim 抄"。
Phase 5 接 drag 之后才发现这条规则有个隐藏前提：lua 用 game-unit
（`G.TILESIZE * G.TILESCALE`，每单位约 30 px），我们 pixel-direct
直接搬 70 当像素用——`max_vel = 70 * dt` 在 60fps 下是 1.17 px/frame，
拖卡释放后 200 px snapback 要跑 170 帧 ≈ 2.8s。

类似的还有 `sway_coeff = 0.015`（VT.r += 0.015 * vel.x / dt），
pixel-direct 下 vel.x 大一个数量级，整张卡飞着转。

直觉对策是按 30× 倍率给单位转换补回来——但具体多少能"对得上 Balatro
手感"还是要 playtest 才知道，硬编 30 等于换种方式硬猜。所以这阶段把
全部手感常数暴露出来，slider 调到爽再决定要不要锁回 constexpr。

## 2 · tuning.h：全局 inline struct

C++17 `inline` 变量就一个文件全搞定，没有 ODR 痛苦：

**Listing 1**: `src/engine/tuning.h`

```cpp
namespace engine::tuning {

struct MovableEase {
  float exp_kxy = 50.0f;
  float exp_kscale = 60.0f;
  float exp_kr = 190.0f;
  float xy_gain = 35.0f;
  float max_vel_pps = 5000.0f;
  float pinch_speed = 8.0f;
  float sway_coeff = 0.0005f;
};

struct HandLayout {
  float w_factor = 0.95f;
  float max_w = 1600.0f;
  float y_offset = 80.0f;
  float bow_factor = 0.4f;
  float highlight_lift = 40.0f;
};

inline MovableEase ease;
inline HandLayout hand;

}  // namespace engine::tuning
```

任何 TU `#include "engine/tuning.h"` 就能读 / 写。Movable.cpp 在
Move() 里读 ease，cardarea.cpp 在 AlignCards 里读 hand，imgui_layer
的 Settings 面板拿地址绑 slider。没有 setter / getter 抽象——MVP
阶段就是想要"改一个值，全游戏立刻生效"。

## 3 · Movable ease 各参数

公式（每帧）：

```text
v = damp * v_old + (1 - damp) * (T - VT) * gain * dt
v = clamp(v, ±max_vel * dt)
VT += v
```

其中 `damp = exp(-exp_k* dt)`，所以 `exp_k` 越大 → damp 越小 → 新力
混入越多 → 速度变化越快。

| 参数 | 默认 | 物理意义 | 调高 / 调低 |
|--|--|--|--|
| **exp_kxy** | 50 | 速度衰减率 (1/sec)：值越大，速度回零越快 | 高：落地脆、过冲少；低：弹簧感强 |
| **xy_gain** | 35 | 误差→力增益：(T-VT) 乘多少入速度 | 高：拉得急；低：温吞 |
| **max_vel_pps** | 5000 | 速度上限 (px/sec)，clamp 速度幅值 | 长距离 snapback 慢就调高；调太低 = drag 释放卡飞行像贴邮票 |
| **exp_kscale** | 60 | VT.scale 追 T.scale 的衰减率 | 影响 juice squash & stretch 的"反弹"节奏 |
| **exp_kr** | 190 | VT.r 追 T.r 的衰减率 | 默认远高于 xy——卡的旋转要比位移先落地，看着才不软 |
| **pinch_speed** | 8 | 翻牌动画恒速（不走 ease） (1/sec) | 越大翻得越快 |
| **sway_coeff** | 0.0005 | 运动时附加旋转：VT.r += sway * vel.x / dt | 0.001+ 卡就打转；0 = 完全不侧倾 |

`max_vel_pps` 是从 lua 的 70 game-unit/sec 重新校准过来的（lua 默认
约等于 70 × 30 px = 2100 px/sec）。我们调到 5000 让 drag 释放更跟手，
还在合理范围内。

`sway_coeff` 同理，0.015 / 30 ≈ 0.0005。lua 的 game-unit 速度量级和
我们 pixel 量级差 ~30 倍。

## 4 · Hand layout 各参数

CardArea 的 hand 弧形 / 选中拔高常数。AlignCards 公式：

```text
slot_x = area.x + (area.w - card_w) * lerp(k)
       + 0.5 * (card_w - card.w)

slot_y = area.y + area.h/2 - card.h/2
       - lift_if_highlighted
       + bow * card.h * bow_factor              ← 弧形
       + 0.03 * card.h * sin(0.666t + slot_x)   ← idle wobble
```

`bow` 范围 [0, 0.25]：中心小、端点大。乘上正的 `bow_factor` → 端点
向下偏移更多 → 中间最高的 ⌒ 扇形。`bow_factor = 0` 给一个完全平的
hand。

| 参数 | 默认 | 说明 |
|--|--|--|
| **w_factor** | 0.95 | hand 宽度占 viewport 的比例 |
| **max_w** | 1600 | hand 宽度硬上限 (px) | 
| **y_offset** | 80 | hand 顶边距 viewport 底部 (px) |
| **bow_factor** | 0.4 | 弧形深度系数 (× card_h)。0 = 平铺；0.4 ≈ 端点比中心低 30 px (card_h=380) |
| **highlight_lift** | 40 | 选中卡向上抬 (px) |

`w_factor + max_w` 是双约束：窄 viewport（1280）下 width=1216；4K
（3840）下 cap 在 1600，避免 hand 撑得过开失去扇形重叠感。

## 5 · ImGui Settings 面板

`ImGuiLayer::DrawSettingsPanel` 把上面两个 struct 用 SliderFloat 暴露
出来。模式直接复用 ImGui 控件 + tuning struct 字段地址：

**Listing 2**: `src/imgui_layer.cpp::DrawSettingsPanel` 节选

```cpp
if (ImGui::CollapsingHeader("Movable ease",
                            ImGuiTreeNodeFlags_DefaultOpen)) {
  auto& e = engine::tuning::ease;
  ImGui::SliderFloat("xy damping", &e.exp_kxy, 1.0f, 500.0f, "%.1f");
  ImGui::SliderFloat("xy gain", &e.xy_gain, 1.0f, 200.0f, "%.1f");
  ImGui::SliderFloat("max velocity (px/sec)",
                     &e.max_vel_pps, 100.0f, 20000.0f, "%.0f");
  // ...
  if (ImGui::Button("Reset Ease")) engine::tuning::ResetEase();
}
```

slider 范围按各参数的合理调节区间挑：xy_gain 默认 35，slider 给
1..200——下限 1 是"几乎不拉"，上限 200 是"硬扯"。`%.5f` 给
sway_coeff 因为它本身只有四五位有效。

> **运行验证**：跑 `./build/card.exe` → View 菜单选 Settings → 默认
> dock 在右侧 tab。拖 xy gain 看卡 add/remove 重排速度变化；拖 max
> velocity 看 drag release snapback 速度变化；拖 bow_factor 看扇形
> 弧度。Reset 按钮回到 struct 默认值。

## Caveats

- **不持久化**：值随 imgui.ini 里的窗口位置一起留着也想过，但
  ImGui 不存自定义状态。Phase 6+ 锁手感后，要么写回 constexpr，要
  么开 `assets/tuning.json` 持久化（参考 themes.json 那套）。
- **没分组绑定**：每个 slider 直接读写全局 struct，没经过
  setter——意味着调一个值立刻全局生效。MVP 阶段没 thread safety
  考虑（单线程 game loop）；以后如果加 worker thread 跑模拟要重新
  审视。
- **slider 范围是猜的**：没基准 playtesting 数据，给的是"看着合理
  的区间"。手感锁定后这些范围会缩到狭窄区间或干脆删掉。
- **juice 的常数还没暴露**：`kJuiceScaleFreq=50.8`、`kJuiceRFreq=40.8`、
  `kJuiceDuration=0.4` 这几个还是 constexpr。原因是 squash & stretch
  的频率改了反而难看，等真要做"不同效果不同 juice 风格"再单拆。
- **bow 跟 lua 的常数偏离**：lua 的 `+ abs - 0.2` 在 G.TILESIZE=32
  下整段 ~3 px 弧。我们 `bow * card_h * 0.4` 给 ~30 px 弧——视觉
  上需要的"明显有扇形"在 pixel-port 下需要这么大。手感锁定后回头
  和 G.TILESCALE port（如果做的话）核对。
