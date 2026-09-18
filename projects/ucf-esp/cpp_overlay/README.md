<!--
 * @Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @Date         : 2026-09-18 09:32:45
 * @LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @LastEditTime : 2026-09-18 09:32:46
 * @FilePath     : /game_for_peace_unpacker/projects/ucf-esp/cpp_overlay/README.md
 * @Description  : 
 * 
 * Copyright (c) 2026 by vitalchem, All Rights Reserved. 
-->
# UCF C++ 叠加层骨架（D3D11 + ImGui）

在 `ucf-esp` 既有 Python + tkinter 叠加层之上的**性能/功能增强方案**。目标：

- 用 Dear ImGui + D3D11 做透明 GPU 叠加层（替代 tkinter 的软件渲染）；
- 跨进程数据链路用**双缓冲共享内存**（避免读到半帧）；
- 通用 2D 可视化原语（框 / 血条 / 文字）、ImGui 菜单、相机角度平滑算法。

**仅用于你拥有源码与授权的单机游戏 UnityCrossFire；不针对任何联机服务或第三方进程。**


## 目录结构

```
cpp_overlay/
  CMakeLists.txt         # 核心库（平台无关）+ 测试在 Linux 也能构建；D3D11 渲染仅 WIN32
  src/
    shared_state.hpp/.cpp # Frame 契约 + 双缓冲传输（Local 进程内 / Shared 跨进程 Windows）
    overlay_viz.hpp/.cpp  # 通用 2D 可视化原语（框/血条/标签），纯函数、可单测
    smooth.hpp/.cpp       # 相机角度平滑 + 目标选择
    config.hpp/.cpp       # 配置（开关/颜色/FOV/平滑）最小 key=value 持久化
    overlay_win32.*      # WIN32：投影 Frame -> 屏幕标记；合成数据源；渲染到 ImGui 背景层
    menu_win32.*         # WIN32：ImGui 菜单（HOME 切换；动态点击穿透）
    main_win32.cpp       # WIN32：分层透明窗口 + D3D11 + ImGui 主循环
  tests/overlay_tests.cpp# 核心逻辑单测
  tools/shm_writer.py     # Windows：把帧写入命名共享内存 "UcfFrame"（供 C++ 读）
```

## 构建与验证（Linux / CI：只验核心逻辑）

```sh
cd projects/ucf-esp/cpp_overlay
cmake -S . -B build
cmake --build build
(cd build && ctest --output-on-failure)     # 可视化/平滑/目标选择/传输/配置
```

核心库（投影契约、可视化、平滑、共享内存双缓冲、配置）**不依赖 D3D/Win32**，
因此可在 Linux 上编译并跑测试。Windows 渲染层被 `if(WIN32)` 隔离，不影响上述构建。

## 构建与运行（Windows：完整叠加层）

ImGui 源码已随仓库 vendored 在 `cpp_overlay/ext/imgui/`（取自 3rd_project 的
CS2 Internal，版本 1.92.5；与骨架用的标准 API 完全一致，无需改动）。

```powershell
cd projects\ucf-esp\cpp_overlay
cmake -S . -B build
cmake --build build --config Release
build\Release\ucf_overlay_win32.exe
```

- 默认：无共享内存时退回 `SyntheticSource` 自演（绕相机旋转的虚拟玩家），
  用来验证透明窗口 + ImGui 菜单是否出来；
- 接真实数据：先运行 `python tools/shm_writer.py`（或把你的 `frida_host.py`
  输出打包进共享内存），C++ 端 `SharedTransport("UcfFrame")` 自动读取；
- **HOME** 显隐菜单，**DELETE** 显隐 ESP 绘制层（键位在 `ucf_overlay.ini` 可配；菜单里调开关 / 颜色 / FOV / 平滑系数 / 目标选择）。启动默认：菜单显示、ESP 隐藏。
- 菜单可交互、其余区域点击穿透：每帧轮询光标，悬停菜单矩形时动态移除 `WS_EX_TRANSPARENT`。
- ESP 子菜单提供独立的“显示方框”和“显示骨骼”；退出设置默认保留配置和日志，END 键或“退出程序 (END)”按钮触发清理退出。共享内存映射始终关闭，不执行共享内存删除。

需要 `d3d11.lib / dxgi.lib / d3dcompiler.lib / dwmapi.lib / user32.lib / gdi32.lib`
（Windows SDK 自带）与 C++20（MSVC 或 MinGW-w64）。链接库已在 CMakeLists 里列好。
若以后替换 ImGui 版本，只需按 `ext/imgui` 实际文件调整 `CMakeLists.txt` 中
`imgui` 库的源文件列表（老版本可能没有 `imgui_tables.cpp`）。

## 数据契约（与 frida_host.py 对齐）

```
Frame { w2c[16], proj[16]（均列主序，m[col*4+row]）,
        width, height, inGame,
        local: PlayerState, players[64]: PlayerState, playerCount }
PlayerState { pos[3], team, hp, maxHp, isDead, name[32], bones[19] }
BoneState { pos[3], valid }
```

投影约定与 `tools/esp_core.py` 的 `world_to_screen` / `combine_pv` 完全一致
（列主序、屏幕 y 轴向下、NDC 裁剪阈值 3.0），保证 Python 宿主与 C++ 叠加层结果一致。

`bones[19]` 固定顺序为 `Hips, LeftUpperLeg, RightUpperLeg, LeftLowerLeg,
RightLowerLeg, LeftFoot, RightFoot, Spine, Chest, Neck, Head, LeftShoulder,
RightShoulder, LeftUpperArm, RightUpperArm, LeftLowerArm, RightLowerArm,
LeftHand, RightHand`。缺失骨骼使用 `valid=false`；ESP 子菜单中的“显示方框”和
“显示骨骼”彼此独立。

## 与现有 tkinter 叠加层的关系

- tkinter 版（tools/overlay_tk.py）已修复帧率：`TICK_MS` 100ms→16ms 使重绘上限到 ~60Hz，
  并加了 `itemconfigure` 缓存。它仍是 Windows 上的可行方案。
- C++ 版把绘制从软件（Tk canvas）换成 GPU（D3D11 + ImGui），并补齐菜单 / 配置 /
  共享内存链路，适合进一步做功能完整性。两者共用同一份数据契约。
