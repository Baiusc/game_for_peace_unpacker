# 第三方 ESP / Aimbot / 菜单 —— 通用技术对比分析

> 范围声明：本文**仅**提取 `3rd_project/` 下若干项目中「ESP / Aimbot / 菜单」三大件的
> **通用架构与算法范式**（叠加层结构、FOV 选靶、角度平滑这类纯数学、ImGui 菜单范式）。
> **不复制**任何与具体游戏强绑定的内容：偏移量、特征码、封包协议、反作弊对抗
> （DMA/驱动/注入/Hook anti-cheat）、autowall、triggerbot 等。
> 唯一用途：反哺 `ucf-esp`（自研**单机**游戏 UnityCrossFire 的调试可视化）。
> 本仓库只服务自有单机游戏；任何面向在线多人游戏的注入/Hook/Aimbot **不在范围**。

---

## 1. 样本与架构分类

| 项目 | 类型 | 叠加层技术 | 数据获取 |
|---|---|---|---|
| CS2 Internal | 内部注入 | Hook 进游戏自身 D3D/ImGui 上下文绘制 | 读游戏进程内存 |
| CS2_Lumen_External | 外部进程 | 自建 D3D11 + ImGui 透明叠加窗口 | 读游戏进程内存（外部） |
| Khytt External v3.0 | 外部进程 | 自建 D3D11 + ImGui 叠加窗口 | 读游戏进程内存（外部） |
| DeltaForce_External | 外部+驱动 | 自建叠加窗口 + 驱动读内存 | 驱动通道读内存 |
| Valorant_External | 外部+驱动 | 自建叠加窗口 + 驱动读内存 | 驱动通道读内存 |
| DeltaForceHack | （大型 SDK 转储，5784 文件） | — | 跳过，非通用件 |

**核心架构结论**：无论内部还是外部，三件套的**软件结构高度同构**——
「数据读取线程」→「双缓冲快照」→「渲染线程用 ImGui DrawList 画」这一步在所有项目里都一样。
区别只在于「内存从哪里来」「画布挂在哪里」。

---

## 2. 三者共享的通用数据流（最重要的一张图）

```
[读取线程] 遍历实体列表 → 过滤(死亡/队伍/可见) → 取坐标/骨骼/血量
                              │  写 back_buffer
                              ▼
                    (std::mutex 保护，双缓冲)
                              │  周期性 swap
                              ▼
[渲染线程] 拷贝 front_buffer → 逐实体 WorldToScreen → ImGui DrawList 绘制
                              │
              ESP 盒/骨骼/血条/名字/距离 / Aimbot 选靶 / 菜单控件
```

- **双缓冲 + mutex**：所有外部项目都用 `back_buffer`（读取线程写）与 `front_buffer`（渲染线程读），
  再用一次 `swap`/赋值切换，避免渲染读一半被改写、也减少锁持有时间。
- **ImGui 画布统一用 `ImGui::GetBackgroundDrawList()` / `GetForegroundDrawList()`**：
  游戏世界图层走 Background（被菜单压在底下），菜单/HUD 走 Foreground/窗口。
- **WorldToScreen 是通用基石**：输入「视图矩阵 × 世界坐标」，输出屏幕坐标；
  盒高 = `feetScreen.y - headScreen.y`、盒宽 = `height * 0.45~0.5` 是通用经验比例。

---

## 3. ESP 通用件对比

| 子功能 | 出现项目 | 通用做法 |
|---|---|---|
| 2D 盒 | 全部 | `AddRect`；常见「描边(黑)+填充(半透明)+彩色主框」三层叠加 |
| 角框(CornerBox) | Lumen | 只画四角短线，更隐蔽 |
| 3D 盒 | Lumen / Valorant | 8 个角点分别 W2S 后连 12 条棱 |
| 骨骼 | 全部 | 骨骼索引对数组（如 `{6,5},{5,4}…`）→ 逐对 `AddLine` |
| 血条 | 全部 | 盒左侧竖条，按 `hp/maxHp` 填充；颜色常做绿→黄→红渐变 |
| 护甲条 | Lumen | 盒右侧另一条 |
| 名字/武器 | Lumen / Khytt / Valorant | 盒顶 `AddText`，带阴影描边（`+1,+1` 偏移画黑底） |
| 距离 | 全部 | 盒底 `[%dm]`，单位换算（UE 用 cm→m） |
| 视线连线(Snapline) | Lumen / Valorant | 屏幕底中心 → 脚底 |
| FOV 圈 | Lumen / Valorant | 以屏幕中心为圆心画圆，半径 = `fov/90 * 半宽` |
| 可见性着色 | Lumen / Valorant / Khytt | 可见=亮色，背后墙=暗色（靠渲染时间差判定，属游戏逻辑，不抄） |
| 彩虹色 | Lumen | `sin(t)` 三通道 → RGB 流动 |
| 水印 | Lumen / Khytt | 左上角小窗，显示 FPS/实体数 |
| 命中标记(HitMarker) | Lumen | 伤害事件 → 浮动 `-N` 文字，带淡出 |
| 雷达 | Valorant | 俯视小地图：世界 XY 按相机 yaw 旋转后映射到圆内 |

**优点（这些项目）**：功能极全、视觉打磨多（渐变/描边/阴影/彩虹）。
**缺点（对我们有启示）**：功能全是围绕「击败其他玩家」设计，很多依赖可见性/穿透/武器状态等
**游戏专属数据**——这些不是通用件，不能、也不该搬。

**对 ucf-esp 的可借鉴点（纯渲染层，与数据来源无关）**：
- 血条绿→黄→红渐变（我们目前按 `kind` 固定色，可加血量维度）。
- 描边+填充三层盒（我们目前只有单层 `AddRect`）。
- 角框 / 3D 盒作为可选盒型。
- 文字阴影描边（名字可读性或 debug 标签更清楚）。
- FOV 圈可视化（我们已有 `fov_deg`，画个圈是廉价的调试辅助）。

---

## 4. Aimbot 通用件对比（纯算法，与「打谁」无关）

所有项目的选靶+瞄准数学**完全一致**，可抽象为与游戏无关的算法：

```
for each candidate:
    if dead or same_team or (vischeck && !visible): skip
    target_pos = bone[bonesel]            # head/chest/pelvis
    ideal = AngleFromTo(eye_pos, target_pos)   # 向量→yaw/pitch
    NormalizeAngle(ideal)                  # yaw 折回 [-180,180], pitch clamp [-89,89]
    fov = hypot(ideal.yaw - cur.yaw, ideal.pitch - cur.pitch)
    if fov < best_fov: best = ideal        # FOV 锥内最近准星者胜出
# 平滑
delta = best - cur; NormalizeAngle(delta)
smoothed = cur + delta / smooth            # lerp：smooth 越大越慢越「人类」
# 施加
A) 外部/legit:  mouse_event(MOVE, dx, dy)   # 经灵敏度换算
B) 内部/rage:   写回视角内存/相机            # 注入式，不抄
```

- **选靶**：FOV 锥（最近准星）+ 可选可见性/队伍过滤 + 骨位选择，三者通用。
- **角度数学**：`AngleFromTo`、角度归一化（`yaw ±360` 折回、`pitch` 夹 ±89）是所有项目都有的工具函数。
- **平滑**：`cur + (target-cur)/smooth` 即一阶 lerp，平滑系数越大越柔和；
  可选「humanized」加随机抖动。这**正是我们 `smooth.cpp` 已有的算法**，验证了我们方向正确。
- **RCS（压枪）**：读 `aimPunch`，反向移动鼠标补偿——属武器手感，与游戏数据绑定，不抄。
- **施加路径 A（外部，鼠标事件）** 是我们**可以借鉴的纯本地计算**范式；
  **路径 B（直接写游戏内存视角）** 属于注入，明确超出 ucf-esp 边界，不抄。

> 注：ucf-esp 的 `smooth.cpp` 与 `select_target()` 已经是这套通用算法在「自有游戏相机」
> 上的合法实现（只算角度、不注入鼠标、不碰第三方进程）。第三方项目印证了该算法范式的通用性。

---

## 5. 菜单通用件对比

| 方面 | 做法 | 出处示例 |
|---|---|---|
| 框架 | 全部 ImGui | 一致 |
| 自定义控件 | `CustomCheckbox` / `CustomColor`（用 `InvisibleButton`+`DrawList` 自绘圆角紫调控件） | CS2 Internal `Menu.cpp` |
| 键位捕获 | `PollKey()` 轮询 `GetAsyncKeyState` → 弹窗「Press a key」→ 写入绑定 | CS2 Internal `Menu.cpp`、Valorant `keybind_button()` |
| 中心化配置 | 全局 `Settings` 命名空间/`struct`，各模块 `extern` 引用 | 全部 |
| 按武器分配置 | `WeaponConfig weapon_configs[4]`（fov/smooth/bone 每武器一套） | Lumen `Aimbot.h` |
| 分区/标签 | 用 `Separator`、分组、水印/安全模式指示 | 全部 |
| 配置持久化 | 我们已用 ini 落盘；第三方多用类似 key=value 或 JSON | — |

**最值得吸收的是「键位捕获弹窗」**：我们目前热键硬编码 `HOME`/`DELETE`，
可借鉴 `PollKey`+`BeginPopupModal` 模式做成**菜单内可重绑定的热键**，存进现有 ini。

---

## 6. 横向优缺点总结

| 维度 | 内部注入式 | 外部叠加式 |
|---|---|---|
| 绘制位置 | 游戏内（与画面天然对齐） | 独立透明窗（需自己 W2S + 对齐） |
| 性能/撕裂 | 随游戏帧 | 独立线程，需双缓冲 |
| 反检测暴露 | 高（改游戏内存/Hook） | 低（只读 + 独立窗），但驱动读内存仍属对抗 |
| 代码复杂度 | 依赖 Hook 框架 | 依赖透明窗 + D3D 初始化 |
| **对 ucf-esp 的启示** | 不采用（不注入） | **我们正是外部叠加式**，范式一致，可直接对标 |

---

## 7. 可借鉴/吸收到 ucf-esp 的具体清单（按优先级）

> 前提：仅吸收「渲染/算法/UI 范式」，**不引入任何游戏专属数据字段或注入逻辑**。
> 我们已有：外部透明 D3D11 叠加、ImGui 菜单、shm→`build_draw_list`、FOV 选靶、角度平滑、ini 持久化。

| 优先级 | 借鉴项 | 对应我们现有 | 改动量 |
|---|---|---|---|
| 高 | **菜单内可重绑定热键**（PollKey+弹窗） | 硬编码 HOME/DELETE | 小（menu_win32 加弹窗，写 ini） |
| 中 | FOV 圈可视化 | `fov_deg` 已有 | 小（菜单开个 checkbox 画圆） |
| 中 | 血条绿→黄→红渐变 | 按 kind 固定色 | 小 |
| 中 | 三层盒（描边+填充+主框）/ 角框 / 3D 盒可选 | 单层 `AddRect` | 中 |
| 中 | 文字阴影描边 | 无 | 小 |
| 中 | 雷达小地图（纯 2D 投影，单机调试也实用） | 无 | 中 |
| 低 | 自定义主题控件（圆角/紫调） | 默认 ImGui | 小（纯美化） |
| 低 | 按武器/场景分配置数组 | 单 `Settings` | 中（需先有「武器」概念） |
| 低 | 水印/距离单位换算美化 | FPS 已有 | 小 |
| 待数据 | 可见性双色、命中标记、伤害数字 | 暂无对应数据 | 大（依赖游戏暴露可见性/伤害事件） |

**明确不吸收**（超出边界，已在第 0 节声明）：
注入/Hook 游戏、DMA/驱动读内存、特征码/偏移、反作弊规避、autowall、triggerbot、
直接写视角内存的「路径 B」瞄准。这些与 ucf-esp「单机调试、不注入、不碰第三方进程」的定位冲突。

---

## 8. 一句话结论

这些第三方项目的价值，在于用**大量真实代码**验证了三件套的「标准范式」：
**双缓冲实体快照 + ImGui DrawList 渲染 + FOV 选靶 + 角度 lerp 平滑 + 中心化可持久化配置 + 键位捕获 UI**。
ucf-esp 的现有骨架已经踩在这条标准范式上；后续只需按第 7 节清单做**渲染层与交互层的打磨**，
而**绝不**引入任何面向在线对抗的逻辑。
