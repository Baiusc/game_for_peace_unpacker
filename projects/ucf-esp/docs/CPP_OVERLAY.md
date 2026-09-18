# C++ D3D11 + ImGui 叠加层（ucf_overlay_win32）

`projects/ucf-esp/cpp_overlay` 下的**性能 / 功能增强方案**，替代 `tools/overlay_tk.py` 的软件渲染。
目标：GPU 透明叠加层、共享内存数据链路、通用 2D 可视化、ImGui 菜单、相机角度平滑。

**仅用于你拥有源码与授权的单机游戏 UnityCrossFire；不针对任何联机服务或第三方进程。**

---

## 1. 构建与运行

### 1.1 核心库（Linux / CI，只验逻辑）
```sh
cd projects/ucf-esp/cpp_overlay
cmake -S . -B build
cmake --build build
(cd build && ctest --output-on-failure)     # 可视化/平滑/目标选择/传输/配置
```
核心库（投影契约、可视化、平滑、共享内存双缓冲、配置）**不依赖 D3D/Win32**，
因此可在 Linux 编译并跑测试。Windows 渲染层被 `if(WIN32)` 隔离，不影响上述构建。

### 1.2 Windows 完整叠加层（Actions 产出 exe）
ImGui 源码 vendored 在 `cpp_overlay/ext/imgui/`（取自 3rd_project 的 CS2 Internal，版本 1.92.5）。
```powershell
cd projects\ucf-esp\cpp_overlay
cmake -S . -B build
cmake --build build --config Release
build\Release\ucf_overlay_win32.exe
```
- 默认：无共享内存时退回 `SyntheticSource` 自演（绕相机旋转的虚拟玩家），验证透明窗口 + ImGui 菜单。
- 接真实数据：先运行 `python tools/shm_writer.py`（或把 `frida_host.py` 输出打包进共享内存），
  C++ 端 `SharedTransport("UcfFrame")` 自动读取。
- **HOME** 显隐菜单，**DELETE** 显隐 ESP 绘制层；启动默认菜单显示、ESP 隐藏。
  菜单里调开关 / 颜色 / FOV / 平滑系数 / 目标选择，底部有 mode/src/players/present_hr 诊断行。
- 需 `d3d11.lib / dxgi.lib / d3dcompiler.lib / dwmapi.lib / user32.lib / gdi32.lib`（Windows SDK）+ C++20。

> 产物静态链接（`/MT`），可直接拷到裸 Win11 虚拟机运行，无需装 VS / SDK / VC++ 运行库。

### 1.3 数据契约（与 frida_host.py 对齐）
```
Frame { w2c[16], proj[16]（均列主序，m[col*4+row]）,
        width, height, inGame,
        local: PlayerState, players[64]: PlayerState, playerCount }
PlayerState { pos[3], team, hp, maxHp, isDead, name[32], bones[19] }
BoneState { pos[3], valid }
```
投影约定与 `tools/esp_core.py` 的 `world_to_screen` / `combine_pv` 完全一致
（列主序、`VP = P*V`、屏幕 y 轴向下、NDC 裁剪阈值 3.0）。详见 `STATIC_REFERENCE.md`。

---

## 2. 模块架构

```
main_win32.cpp
  ├─ init_d3d11(hwnd)     创建设备 + 交换链 + 透明窗口机制
  ├─ frame()              每帧：读共享内存 → project_frame → build_draw_list
  │                        → ImGui 菜单/HUD → ClearRenderTargetView → Present
  └─ wndproc              ImGui 输入处理

overlay_win32.*  project_frame()   复刻 esp_core 的列主序投影 + NDC 裁剪 → ScreenMark[]
                  SyntheticSource  无数据时自演
                  render_draw_list() 把 2D 原语画到 ImGui 背景绘制层
shared_state.*   Frame/PlayerState 契约 + 双缓冲传输（Local / Shared）
overlay_viz.*    build_draw_list() 视口映射 + 框/血条/标签原语
smooth.*         smooth_angles() / select_target()（只算角度，不注入）
config.*         key=value 配置持久化（ucf_overlay.ini）
menu_win32.*     ImGui 菜单（HOME/DELETE 切换；缓存菜单矩形给点击穿透命中判定）
```

---

## 3. 透明窗口机制（关键，见排错）

窗口必须是**点击穿透**（`WS_EX_TRANSPARENT`）+ **永远置顶**（`WS_EX_TOPMOST`）+ **不抢焦点**
（`WS_EX_NOACTIVATE`）的弹出层。透明靠交换链的 alpha，**不是**靠分层窗口属性。

- **首选 flip 模型**：`WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE`
  （**不要**带 `WS_EX_LAYERED`），交换链 `FLIP_DISCARD + DXGI_ALPHA_MODE_PREMULTIPLIED`，
  配合 `DwmExtendFrameIntoClientArea(hwnd, {-1})` 把桌面透出；每帧清屏 `(0,0,0,0)`（透明）。
- **回退 blt 模型**（虚拟机不支持 flip 时）：补加 `WS_EX_LAYERED` +
  `SetLayeredWindowAttributes(hWnd, RGB(0,0,0), 0, LWA_COLORKEY)`（纯黑像素=透明），
  每帧清屏 `(0,0,0,255)`（不透明黑，交给 colorkey 抠掉）。

日志首行判定走哪条路：
```
init: flip_hr=0x00000000 used=1 blt_hr=0x00000000 dwm_hr=0x00000000 clear=transparent   # flip 成功
init: flip_hr=0x887A0001 used=0 blt_hr=0x00000000 dwm_hr=0x00000000 clear=black-colorkey # blt 兜底
```
心跳行含 `present_hr=0x00000000` 表示 `Present` 正常。

---

## 4. 排错（实测）

### ① 日志全绿但屏幕上什么都看不到 —— 窗口整体不可见（头号坑）
**现象**：`[1] mode=BLT menu=1 win=1832x904 frame=1280x720 players=13 scale=1.43 src=synth`
一切正常，但屏幕无任何内容。

**根因**（两个阶段，叠在一起）：
1. `CreateWindowEx` 带了 `WS_EX_LAYERED`，但代码从未调用
   `SetLayeredWindowAttributes` / `UpdateLayeredWindow` 去定义分层属性 →
   **分层窗口默认整体不可见**。
2. 同时 `WS_EX_LAYERED` 与 **flip 模型交换链互斥**，导致 `CreateSwapChainForHwnd`
   返回 `0x887A0001`（`DXGI_ERROR_INVALID_CALL`），被迫回退 BLT；
   BLT + `UNSPECIFIED` 缓冲不透明，叠加「未定义分层属性」→ 仍不可见。

**修法**（commit `afcc12a`）：
- 窗口创建**去掉 `WS_EX_LAYERED`** → flip 交换链能成功创建，靠 swap chain 自身 alpha 做逐像素透明。
- flip 失败才在回退路径补加 `WS_EX_LAYERED + LWA_COLORKEY`（纯黑透明）兜底，并显式
  `SetLayeredWindowAttributes`，保证 BLT 路径窗口可见。

> DXGI 硬性规则：**flip 模型交换链禁止与 `WS_EX_LAYERED` 同用**，否则 `0x887A0001`。

### ② scale=inf（早前已修）
无有效共享内存时 `frame.width/height` 可能为 0/负，视口缩放 `cw/fw` 分母=0 → `inf`。
修法：`fw = (f.width>0)? f.width : 1280.0f`（分母兜底），并校验 `f.width>0 && f.height>0`
才认为有数据，否则退回 `SyntheticSource` 自演。

### ③ 诊断三宝（验证渲染管线是否真的画到屏）
- **强制红框**：`bdl->AddRectFilled(ImVec2(100,100), ImVec2(600,500), IM_COL32(255,0,0,255));`
  看到红方块 = 渲染管线已上线（最直白的探针）。
- **高对比 HUD**：黑底（`IM_COL32(0,0,0,230)`）+ 白字（`IM_COL32(255,255,255,255)`）。
- **每帧 `present_hr`** 写入 `ucf_debug.log` 心跳行，确认 `Present` 没失败。

> 2026-09-18：透明窗口在 VM 里验证通过后，全屏 HUD 与强制红框已从代码移除，
> 诊断信息收进菜单窗口底部；需要复测时临时加回即可。

### ④ 判别清单（在虚拟机里看）
| 看什么 | 期望 | 说明 |
|---|---|---|
| 屏幕上有没有**红方块** | 有 | 渲染管线通了，透明已生效 |
| `init:` 首行 `used=` | `1`/`0` | flip 成功 / blt 兜底 |
| `init:` 首行 `clear=` | `transparent`/`black-colorkey` | 清屏模式 |
| 心跳 `present_hr` | `0x00000000` | Present 正常 |
| 任务栏有 `UCF Overlay`、鼠标能穿透 | 是 | 点击穿透正常 |

- `used=1` 且看到红方块 → 透明 OK。
- `used=0 clear=black-colorkey` 且仍看不见 → BLT colorkey 在你的 VM 没生效，把 `init:` 与几行心跳发回，改 BLT 用 `LWA_ALPHA` 不透明兜底。

### ⑤ 菜单点不中，点到背后的窗口（点击穿透）
**现象**：透明窗口跑通后，点击菜单按钮落到了背后的文件管理器。

**根因**：窗口常开 `WS_EX_TRANSPARENT` → 整窗对鼠标不可见，所有点击（包括菜单上）
都穿透到底下窗口。

**为什么不 `WM_NCHITTEST` 返回 `HTTRANSPARENT`**：它只保证**同线程**窗口间的穿透，
跨进程（游戏 / 文件管理器）不可靠，标准做法是动态切换 ex-style。

**修法**（`update_click_through()`，每帧执行）：
1. `draw_menu` 缓存 ImGui 菜单窗口矩形（`MenuRect`，有一帧延迟，可接受）；
2. 主循环 `GetCursorPos` 轮询，光标落在菜单矩形（外扩 8px）内 → **移除** `WS_EX_TRANSPARENT`；
3. `ImGui::IsAnyMouseDown()` 为真（拖滑块）→ 保持可交互，避免拖到矩形外被断掉；
4. 其余时间恢复穿透；切换时补 `SWP_FRAMECHANGED` 让系统重算命中。

**注意**：带 `WS_EX_TRANSPARENT` 时收不到任何鼠标消息，所以「光标进入菜单」只能靠
轮询 `GetCursorPos` 探测，不能等 `WM_MOUSEMOVE`。

### ⑥ 菜单文字全显示 `????`（ImGui 默认字体无 CJK）
**现象**：菜单里中文标签全变 `????`。
**根因**：ImGui 默认字体（proggyClean 等）不含 CJK 字形，`/utf-8` 只解决源码编码，不解决字形。
**修法**：`main_win32.cpp` 的 `init_overlay_fonts()` 在 `ImGui::CreateContext()` 后运行时加载系统 CJK 字体
（`C:/Windows/Fonts/msyh.ttc` 等候选，用 `GetGlyphRangesChineseFull`），菜单可正常显中文；
加载失败（如 VM 缺字体文件）则保留默认 ASCII 字体兜底。屏上 ESP 框标签仍用 ASCII 的 kind/hp/距离，
避免破坏 `overlay_tests` 对标签格式的契约断言。

---

## 5. 后续可扩展
- 距离标注、`dist = |local.pos - p.pos|`。
- 骨骼 / 更精细包围盒：从 `Transform` 层次读更多关节。
- 性能：已 GPU 化；可加帧率上限 / 脏矩形。
- 真机校准：拿到真实矩阵后，对照已知世界点微调 FOV / 宽高，消除投影偏差。

### 4.⑦ 菜单与纯角度目标输出

- `Settings` 仍使用兼容的 `key=value` 文件；菜单控件变化后即时写回 `ucf_overlay.ini`，HOME 切换菜单，DELETE 切换 ESP 绘制层。
- 绘制设置通过 `DrawStyle` 传入 `build_draw_list()`，框、血条、距离、骨骼、颜色、最大距离和线宽不会散落在渲染层。
- `smooth` 的目标选择支持距离、最低血量、准星角度三种模式；主循环只计算并显示 yaw/pitch，未调用任何鼠标或输入 API。
- `PlayerState.bones[19]` 使用固定 Humanoid 槽位；无效骨骼用 `valid=false`，C++ 只连接两端都成功投影的骨骼。真实读取链是 `characterAnimator -> GetBoneTransform -> position_Injected`。
- 退出路径默认保留 `ucf_overlay.ini` 和 `ucf_debug.log`；END/菜单按钮会保存配置、销毁窗口与 ImGui/D3D11，并让 `SharedTransport` 析构执行 `UnmapViewOfFile + CloseHandle`。只有用户勾选对应退出选项时才删除配置或日志。
