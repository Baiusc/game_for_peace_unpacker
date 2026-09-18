# 3rd 项目的鼠标移动方式 vs ucf-esp 的 L1 方案

> 用途：只读分析 `3rd_project` 里各项目**实际如何驱动鼠标移动 / 视角转动**，对照
> `ucf-esp` 的本地模拟方案（根 AGENTS.md §0.1 授权、只用 L1 `SendInput`/`mouse_event`）。
> **不复制任何偏移 / 驱动加载 / 反作弊对抗逻辑，只提炼"移动发生在哪一层"。**

---

## 0. 层级速查（与 §0.1 的关系）

| 层 | 方式 | ucf-esp §0.1 是否允许 |
|---|---|---|
| **L1 用户态合成** | `SendInput` / `mouse_event`（`MOUSEEVENTF_MOVE`） | ✅ 允许（唯一允许） |
| L1.5 光标直设 | `SetCursorPos` / `MOUSEEVENTF_MOVE_NOCOALESCE` | ✅ 允许（但 FPS 相机基本无效） |
| L2 驱动通信 | `DeviceIoControl` 给鼠标驱动发 IOCTL | ❌ 越界（需装额外驱动） |
| L3 内核驱动 | 自写驱动塞 HID 栈 | ❌ 违反 §0.1 |
| L4 硬件设备 | Arduino / KMBox | ❌ 越界 |
| **L5 内存直写** | 写 `yaw`/`pitch`/`Camera` 字段 | ❌ **明确红线** |

**关键发现：3rd 项目绝大多数走 L5（写视角内存）或 L2/L3（驱动），只有 CS2_Lumen 的 legit 模式是 L1。**
也就是说，`ucf-esp` 选的 L1 是这些项目里**最轻、最保守、唯一符合 §0.1** 的一层。

---

## 1. 3rd 各项目移动方式总览（带 file:line）

| 项目 | move/trace 手段 | 所在层 | 触发按键 | 备注 |
|---|---|---|---|---|
| **CS2_Lumen_External** | `mouse_event(MOUSEEVENTF_MOVE, x, y)` **或** `Write(dwViewAngles)` | **L1 + L5 双模式** | 按住 `aim_key` | `target_legit` 切换；非 legit 走内存写且受 `safety_lock` 门控 |
| **Khytt External** | `SetViewAngles()` → `WriteProcessMemory(ClientBase+dwViewAngles)` | **L5** | 按住 `target_key_code` | 经典 external 内存写 |
| **Valorant_External** | `write(PlayerController+0x448, TargetAngle)`（视角内存写）；trigger 用 `mouse_event(LEFTDOWN/UP)` | **L5 + L1(仅开火)** | 按住 `AimKeyList[keyselect]`（含侧键） | 视角走 L5；只有点击开火走 L1 |
| **DeltaForce_External** | `MouseMoveR()` → `DrvCom(IOCTL_KERNEL_MOUSE)` 发内核鼠标驱动 | **L2/L3 驱动** | 按住右键 `GetAsyncKeyState(2)` | 经驱动 IOCTL 模拟硬件级输入 |

---

## 2. 逐项目展开

### 2.1 CS2_Lumen_External —— L1 + L5 双模式（最贴近我们的对照）

`Features/Aimbot.h` 里同一套选靶结果，有两条应用路径：

- **L1（legit）**：`Main.cpp:87` `target_legit = true; // Uses mouse_event`，
  在 `Aimbot.h:218-219` 把角度差换算成像素移动，`:237` 发 `mouse_event(MOUSEEVENTF_MOVE, (int)x_move, (int)y_move, 0, 0)`。
  换算公式（关键，L1 必做）：
  ```cpp
  float m_yaw = 0.022f * Settings::sensitivity;          // 角度→像素比例
  float x_move = (angle_step.y / m_yaw) / smooth;        // Aimbot.h:218
  float y_move = (angle_step.x / m_yaw) / smooth;        // Aimbot.h:219
  x_move = std::max(-40.f, std::min(40.f, x_move));      // 单事件限幅 Aimbot.h:228
  mouse_event(MOUSEEVENTF_MOVE, (DWORD)(int)x_move, (DWORD)(int)y_move, 0, 0);
  ```
- **L5（non-legit）**：`Aimbot.h:248-254` 直接 `g_Memory->Write<Vector3>(g_Client + g_Off.dwViewAngles, smoothed)` 写视角内存；该路径被 `safety_lock`（`Aimbot.h:239`）门控。

> **对 ucf-esp 的意义**：CS2_Lumen 的 legit 模式就是我们要的 L1 行为模板——只是它基于**角度空间**（`angle_step`），而我们是**屏幕空间**（`ScreenMark` 减屏心）。换算时我们直接用屏幕像素差，不需要 `m_yaw` 那步。

### 2.2 Khytt External —— 纯 L5（内存写）

`src/sdk/game.h:92-96`：
```cpp
inline void SetViewAngles(const Vector3 &angles) {
  Memory::Write<Vector3>(Memory::ClientBase + cs2_dumper::offsets::client_dll::dwViewAngles, angles);
}
```
`src/sdk/memory.h:85-90` 的 `Write` 底层是 `WriteProcessMemory(ProcessHandle, ...)`。
调用点 `aimbot.cpp:133-134`：`if (GetAsyncKeyState(target_key_code)) Game::SetViewAngles(best_angle);`

→ 这是典型的 **external 写入游戏进程内存**（L5），与 §0.1「不写游戏内存」红线冲突，**ucf-esp 不采用**。

### 2.3 Valorant_External —— L5 视角 + L1 开火

- 视角：`cheat.hpp:1296-1300` 选中 `AimKeyList[keyselect]`（含 `VK_XBUTTON1/2`）后，
  `write<FVector>(PlayerController + 0x448, TargetAngle)` 直接写控制旋转（L5）。
- 开火：`cheat.hpp:1207-1238` 的 `bTrigger` 在准星命中骨骼（像素距 `<4`）时
  `mouse_event(MOUSEEVENTF_LEFTDOWN/UP)`（**唯一的 L1 用法，且只用于点击**）。
- 另带 `Driver/driver.hpp` + `kdmapper`：用**漏洞驱动**取得读写游戏内存的权限（这是它能做 L5 的前提），本身已属越界。

→ 视角是 L5，点击是 L1。**我们不取 L5；若未来要做 triggerbot，可只借它的 L1 点击思路**（已在 §0.1 授权内）。

### 2.4 DeltaForce_External —— L2/L3 驱动 IOCTL

`Game/Engine.cpp:487-490`：`if (GetAsyncKeyState(2)) ProcessMgr.MouseMoveR(AimPos.x, AimPos.y);`
`MouseMoveR` 定义于 `Driver.hpp:125-173`：构造一个 `MOUSE_INPUT_DATA` 形态的结构体
（`UnitId/Flags/Buttons/LastX/LastY`），经 `DrvCom(Dr_Handle, &msgInfo, ..., IOCTL_KERNEL_MOUSE)` 发往**内核鼠标驱动**。

→ 这是 **L2/L3 驱动级硬件模拟**，游戏收到「无注入痕迹」的真实硬件输入。需要本机装/加载该驱动，
明显超出「自研单机 + 本机 + 不装额外组件」的干净边界，**§0.1 已排除**。

---

## 3. 角度空间 vs 屏幕空间：L1 换算的差异

| | CS2_Lumen（角度空间） | ucf-esp（屏幕空间） |
|---|---|---|
| 选靶度量 | `fov = sqrt(dyaw²+dpitch²)` | FOV 圆内距屏心最近（`select_screen_target`） |
| 本帧位移来源 | `angle_step = best - current_angles` | `dx = target_screen.x - center.x`；`dy = target_screen.y - center.y` |
| →像素换算 | `x_move = (angle_step.y / m_yaw) / smooth` | **无需 `m_yaw`**，dx/dy 本身就是像素 |
| 发送 | `mouse_event(MOUSEEVENTF_MOVE, x_move, y_move)` | `SendInput`/`mouse_event` 发 `(dx/smooth, dy/smooth)` |

> ucf-esp 用屏幕空间选靶，**反而省掉了角度→像素的比例换算**，L1 实现更直接。
> 只需注意：单事件位移要限幅（CS2_Lumen 限 `[-40,40]`），避免一次跳太大被游戏/系统吞掉。

---

## 4. 对比结论

1. **3rd 项目几乎都不只用 L1**：CS2_Lumen 有 L1+ L5 双模；Khytt、Valorant 走 L5（写视角内存）；DeltaForce 走驱动（L2/L3）。
2. **ucf-esp 选的 L1 是其中唯一符合 §0.1 的层**，最干净、最无需额外组件、最不易"越界"。
3. **L1 的代价**（对在线游戏是弱点，对自研单机无影响）：合成事件带 `LLMHF_INJECTED` 标志，反作弊可直接识别并丢弃。但你的场景是「关闭反作弊的调试构建」，**这个代价不存在**。
4. **"trace"在 L1 下只是行为模式**：按住侧键期间每帧发一条 `MOUSEEVENTF_MOVE`（或 `SendInput`），让准星逐步贴上目标——不是独立技术层。
5. **可借鉴的只有 CS2_Lumen 的 L1 部分**：像素限幅、平滑系数、`m_yaw`-式换算（我们简化成直接像素差）。其余 L5/驱动部分一律不吸收。

---

## 5. 对 ucf-esp `input_sim/` 的落地提示（仅文档，不改代码）

- 移动只用 `SendInput(INPUT_MOUSE, MOUSEEVENTF_MOVE, dx, dy)`；`mouse_event` 作兼容降级。
- `dx/dy` 直接来自 `select_screen_target` 的屏幕坐标减屏心，**不再做角度→像素换算**。
- 每帧位移除以平滑系数、并限幅（如 `[-40,40]` 像素/事件）。
- 按住侧键 = 每帧重复上述（trace）；点按侧键 = 单次 move + 对齐后 `LEFTDOWN/UP`（triggerbot 思路，源自 Valorant 的 L1 点击用法，但走我们自己的 §0.1 路径）。
- **绝不**调用 `WriteProcessMemory`、驱动 IOCTL、或写任何游戏内存（L5/L2-L4 全排除）。

---

## 6. 文件索引（3rd 引用）

- `3rd_project/CS2_Lumen_External/Features/Aimbot.h`（L1 `mouse_event` 237 行；L5 `Write` 254 行；`m_yaw` 换算 218-219）
- `3rd_project/CS2_Lumen_External/Main.cpp:87`（`target_legit` 开关）
- `3rd_project/Khytt External v3.0/src/sdk/game.h:92-96` + `src/sdk/memory.h:85-90`（`SetViewAngles`→`WriteProcessMemory`，L5）
- `3rd_project/Khytt External v3.0/src/features/aimbot.cpp:133-134`（调用点）
- `3rd_project/Valorant_External/Game/cheat.hpp:1296-1300`（L5 视角写）、`1207-1238`（L1 开火点击）、`706-711`（isvisible）
- `3rd_project/DeltaForce_External/DeltaForce_External/Game/Engine.cpp:487-490` + `Driver.hpp:125-173`（`MouseMoveR`→驱动 IOCTL，L2/L3）
