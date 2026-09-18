# 3rd 项目通用选靶方案对照（只读参考 / 仅取通用算法）

> 边界：本文件只分析 `3rd_project` 中 **ESP / Aimbot / 菜单** 三件的**通用算法与架构模式**，
> **不记录**任何游戏专属偏移、特征码、封包或反作弊对抗手法（仓库约定，见 `AGENTS.md` 第 0 节）。
> `ucf-esp` 服务自研单机游戏，Aimbot 仍只做目标选择 + 角度计算 + 可视化；不写游戏内存/不 Hook。
> 本地鼠标模拟（SendInput/mouse_event）按根 AGENTS.md §0.1 授权，为独立 `input_sim/` 模块、默认 OFF。

---

## 0. 总览：3rd 通用选靶 = 两阶段，两套方案

所有项目都先把"不可选"目标过滤掉，再在候选里按一个**度量**挑一个：

```
阶段 1 过滤：队伍 / 死亡 / 可见性 / 最大距离   （三项目共有，udf-esp 已实现）
阶段 2 选靶：在 FOV 内挑"离准星最近"的目标      （两套通用方案，见下）
```

选靶阶段有**两套通用方案**，差别只在"最近"用什么度量：

| 方案 | 度量空间 | 代表项目 | 是否需要投影坐标 |
|---|---|---|---|
| **A. 角度空间 FOV 锥** | 视角角度差（度） | CS2_Lumen_External、Khytt External | 否（用世界坐标 + 当前视角） |
| **B. 屏幕空间距屏心** | 投影后像素距离（px） | Valorant_External | 是（复用 ESP 的 w2s 结果） |

两者数学上**近中心等价**（透视投影下 `像素偏移 ≈ 焦距·tan(θ) ≈ 焦距·θ`，小角度时最小角度差 ≈ 最小像素距离），
只是度量与依赖不同。

---

## 1. 方案 A：角度空间 FOV 锥（CS2_Lumen_External / Khytt）

### 1.1 CS2_Lumen_External — `Features/Aimbot.h`

```120:177:3rd_project/CS2_Lumen_External/Features/Aimbot.h
        Vector3 current_angles = g_Memory->Read<Vector3>(g_Client + g_Off.dwViewAngles);
        int local_team = g_Memory->Read<int>(local_pawn + g_Off.m_iTeamNum);

        float best_fov = cfg.fov;
        Vector3 best_target_angles = { 0.f, 0.f, 0.f };
        int best_target_index = -1;
        ...
            float delta_yaw   = ideal_angles.y - current_angles.y;
            float delta_pitch = ideal_angles.x - current_angles.x;

            while (delta_yaw > 180.f)  delta_yaw -= 360.f;
            while (delta_yaw < -180.f) delta_yaw += 360.f;

            float fov = sqrtf(delta_yaw * delta_yaw + delta_pitch * delta_pitch);
            if (fov < best_fov) {
                best_fov = fov;
                best_target_angles = ideal_angles;
```

- `cfg.fov` 是 FOV 锥半角（度）；`best_fov` 初值即锥半径，越小越严。
- 度量：`fov = sqrt(dyaw² + dpitch²)`（视角空间欧氏距离）。
- 选最小 `fov` 且在锥内者。

### 1.2 Khytt External — `src/features/aimbot.cpp`

```47:107:3rd_project/Khytt External v3.0/src/features/aimbot.cpp
  float best_fov = cfg.fov;
  Vector3 best_angle = {0, 0, 0};
  uintptr_t best_target = 0;
  ...
    Vector3 angle = CalculateAngle(eye_pos, target_pos);
    float fov = CalculateFov(eye_angles, angle);

    if (fov < best_fov) {
      best_fov = fov;
      best_angle = angle;
      best_target = player;
    }
```

- 逻辑同 Lumen：`CalculateAngle` 算到目标的视角，`CalculateFov` 算与当前视角的角差，取最小且在 `cfg.fov` 内。

### 1.3 方案 A 优缺点

- **优点**
  - 与分辨率 / 宽高比无关，纯角度度量。
  - 直接对应"游戏 FOV 锥"，语义清晰（谁在准星锥内、谁更居中）。
  - 不需要先算投影坐标，适合内部注入或没画 ESP 的场景。
- **缺点**
  - 需要当前视角 + 视点世界坐标 + 目标世界坐标（多一次世界读取）。
  - 要 `ToAngle` + 角度归一化（`±180` 缠绕），略多计算。
  - 与"屏幕上画出来的 FOV 圆"不是同一坐标系，需额外换算才能和可视化对齐。

---

## 2. 方案 B：屏幕空间距屏心最近（Valorant_External）

### 2.1 Valorant_External — `Game/cheat.hpp`（CheatLoop）

```1240:1250:3rd_project/Valorant_External/Game/cheat.hpp
			FVector head = LABNMTRX(8, ValEntityList);
			FVector head_w2s = UE4::SDK::ProjectWorldToScreen(head);

			float delta_x = head_w2s.x - (Width / 2.f);
			float delta_y = head_w2s.y - (Height / 2.f);
			float dist = sqrtf(delta_x * delta_x + delta_y * delta_y);
			float fovdist = CalculateDistance(Width / 2, Height / 2, head_w2s.x, head_w2s.y);
			if ((dist < closest_distance) && fovdist < Settings::Visuals::FovValue) {
				closest_distance = dist;
				closestplayer = x;
			}
```

- 先 `ProjectWorldToScreen` 把头部投到屏幕（ESP 也用这步，复用）。
- 度量：`dist = sqrt(dx² + dy²)`（屏心到投影点的像素距离）。
- 选最小 `dist` 且在 `FovValue` 半径内者（`fovdist` 检查与 `dist` 实际等价，属冗余保险）。

### 2.2 方案 B 优缺点

- **优点**
  - **直接复用 ESP 的投影坐标**，零额外世界读取。
  - 选靶圆 = 屏幕上画的 FOV 圆，**所见即所选**，调试直观。
  - 实现最短（几个像素减法 + sqrt）。
- **缺点**
  - 与分辨率 / 宽高比绑定：FOV 半径要以像素计、随窗口缩放。
  - 依赖投影点本身（这里取头部 bone 8），取点不同结果不同。

---

## 3. 等价性与取舍

- 标准透视投影下，`屏幕偏移 ≈ 焦距·tan(视角偏移) ≈ 焦距·视角偏移`（中心附近），
  故 **方案 A 最小角差 ≈ 方案 B 最小像素距离**（同一取点，如都取头部）。
- 行为一致，差异在**度量单位与依赖**：
  - A 用"度"，与屏幕无关，需世界坐标。
  - B 用"像素"，需投影坐标，天然贴合外部叠加层已算好的 w2s。
- **结论**：外部叠加层（即 `ucf-esp` 的形态）首选 B——我们本就有投影坐标，且能令"可视化 FOV 圆"与"选靶圆"完全同公式。

---

## 4. ucf-esp 现状对照

- 选靶实现：`smooth.cpp` 的 `select_screen_target`（屏幕空间，方案 B）：

```75:92:projects/ucf-esp/cpp_overlay/src/smooth.cpp
int select_screen_target(const ScreenCandidate* c, int n,
                         float center_x, float center_y, float fov_radius) {
    if (!c || n <= 0 || fov_radius <= 0.0f) return -1;
    const float radius2 = fov_radius * fov_radius;
    int best = -1;
    float best_distance2 = radius2;
    for (int i = 0; i < n; ++i) {
        if (!c[i].valid) continue;
        const float dx = c[i].x - center_x;
        const float dy = c[i].y - center_y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 <= best_distance2) {
            best = i;
            best_distance2 = distance2;
        }
    }
    return best;
}
```

- FOV 半径公式（可视化圆与选靶圆同一来源）：`main_win32.cpp:458`
  `fov_radius = (fov / 90.0f) * (min(w,h) * 0.5f)`，菜单 `FOV` 滑块直接驱动。
- 过滤链（队伍/死亡/可见/最大距离）在调用前完成，与 3rd 通用做法一致。
- 选靶后仍用**世界坐标**算只读 yaw/pitch（`smooth.cpp` 角度计算函数）；不写游戏内存/不 Hook（本地鼠标模拟见根 AGENTS.md §0.1）。
- 顺带：`smooth.cpp:60-70` 还保留了 `mode` 分支（0=准星角度最近 / 1=最低血量），说明我们代码同时具备**方案 A 的角度度量**能力，只是默认走方案 B。这是比 3rd 更灵活的一点（可作为菜单可选项）。

### 与 3rd 的差异（非缺陷，是符合本仓库定位的裁剪）
- 不读游戏内存、不取 bone 偏移：`PlayerState` 当前是单点（无头/胸区分），取点固定——对自研单机游戏足够。
- 不写游戏内存/不 Hook/不注入到游戏进程：3rd 项目选靶后通常接鼠标写入；我们只做可视化与角度输出。
  本地鼠标模拟（SendInput/mouse_event，含 triggerbot 式自动左键）按根 AGENTS.md §0.1 授权，须为独立 `input_sim/` 模块、默认 OFF。
- 不复制任何 3rd 的偏移/特征/反作弊逻辑，仅借鉴上述通用度量模式。

---

## 5. 可借鉴 / 已对齐
- ✅ 过滤链（team/dead/visible/max-dist）—— 与三项目共有，已对齐。
- ✅ 选靶 = FOV 内距准星最近 —— 采用方案 B（屏幕空间），与 Valorant 一致，且可视化/选靶同圆。
- 🔶 度量可配置（角度最近 / 最低血量 / 屏心最近）—— 我们 `mode` 已有雏形，可作为菜单"选靶模式"暴露。
- 🔶 取点（头/胸/骨盆）选择 —— 3rd 普遍支持 bone 选择；待 `PlayerState` 补全多骨点后再接，属数据契约扩展。

## 6. 当前已落地的可配置扩展

`ucf-esp` 现已将两种通用度量都接入配置：

- `aim_selection_mode=0`：屏幕空间，FOV 圆内距屏幕中心最近；
- `aim_selection_mode=1`：角度空间，使用相机前向量和目标角度差，在角度 FOV 内取最小角差。

取点也已配置化：

- `aim_point_mode=0`：身体中心 `PlayerState.pos`；
- `aim_point_mode=1`：头部 `bones[10]`；
- `aim_point_mode=2`：胸部 `bones[8]`；
- `aim_point_mode=3`：指定 `aim_bone_id` 槽位。

取点先参与过滤和选靶，再用于只读 yaw/pitch 计算；骨骼点无效时不会伪造目标点。整个流程不写游戏内存/不 Hook（本地鼠标模拟见根 AGENTS.md §0.1，独立模块、默认 OFF）。
