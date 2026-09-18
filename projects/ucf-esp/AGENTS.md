# ucf-esp 子项目 — Agent 指南

> 本文件是 CodeBuddy / CODEX / 豆包 / WorkBuddy 进入 `projects/ucf-esp` 后的**必读**。
> 根级约定（边界 / 合规 / 推送流程）见仓库根 `AGENTS.md`。

## 这是什么

你自己的**单机游戏 UnityCrossFire**（Unity IL2CPP，32-bit）的调试工具集合。两种运行时叠加层实现：

1. **Python + tkinter**（`tools/overlay_tk.py` + `tools/frida_host.py`）：已可用，
   含 DPI 感知、坐标映射(stretch/letterbox)、失焦隐藏、点击穿透、防闪烁、跨线程队列。
2. **C++ D3D11 + ImGui**（`cpp_overlay/`）：性能 / 功能增强骨架，
   GPU 渲染 + 共享内存数据链路（`frida_host.py --shm` 喂真实帧）+ 菜单 / 配置 + 平滑算法。
   真实数据链路代码层已通（`UcfFrame` 双缓冲写/读 + `tests/test_shm_bridge.py`），待实机验证。

两者共用同一份**数据契约** `Frame`（列主序矩阵 + 玩家数组），保证 Python 与 C++ 投影结果一致。

## 目录

```
projects/ucf-esp/
  tools/        # Python 宿主：frida_host.py / overlay_tk.py / esp_core.py /
               #   frida_dump.js(+.bundle.js) / shm_writer.py / replay.py / frida_probe.py
  tests/        # 离线单测：进程选择 / 契约 / 投影校准 / 叠加层几何
  il2cpp_dump/  # 游戏 dump.cs 等
  docs/         # HOST_OVERLAY / STATIC_REFERENCE / STATIC_TRIAGE / CPP_OVERLAY / PITFALLS
  cpp_overlay/  # C++ D3D11 + ImGui 叠加层（本文件重点）
  src/          # 离线投影实验室 ucf_projection_lab（C++20，合成数据）
```

## cpp_overlay 模块划分

| 文件 | 职责 | 平台 |
| --- | --- | --- |
| `src/shared_state.hpp/.cpp` | `Frame`/`PlayerState` 契约 + 双缓冲传输（`LocalTransport` 进程内 / `SharedTransport` 跨进程 Windows 共享内存） | 核心（Linux 可编） |
| `src/overlay_viz.hpp/.cpp` | 通用 2D 原语（框/血条/标签）+ `build_draw_list` 视口映射 | 核心 |
| `src/smooth.hpp/.cpp` | 相机角度指数平滑 + 目标选择（只算角度；不写游戏内存/不 Hook；本地输入模拟见根 AGENTS.md §0.1 独立模块） | 核心 |
| `src/config.hpp/.cpp` | key=value 配置持久化 | 核心 |
| `src/overlay_win32.*` | Frame→屏幕标记投影（复刻 `esp_core`）+ 合成数据源 `SyntheticSource` + `render_draw_list` 画到 ImGui 背景层 | WIN32 |
| `src/menu_win32.*` | ImGui 菜单（HOME/DELETE 切换；动态点击穿透） | WIN32 |
| `src/main_win32.cpp` | 分层透明窗口 + D3D11 + ImGui 主循环 | WIN32 |

构建 / 运行 / 数据契约见 `cpp_overlay/README.md`；
**透明窗口与 flip/blt 踩坑见 `docs/CPP_OVERLAY.md`**。

## 关键步骤（agent 改代码时）

1. 改数据契约 → 同步 `shared_state.hpp` + `esp_core.py` + `frida_dump.js` + `STATIC_REFERENCE.md`，
   跑 `tests/test_dump_contract.py` 与 cpp_overlay ctest。
2. 改 C++ 逻辑 → 本地 `cmake -S . -B build && cmake --build build && (cd build && ctest --output-on-failure)`
   （Linux 可验证核心库；注意 ctest 要在 build 目录内跑，`--test-dir` 在本机版本下不可靠）；Windows 渲染层靠 Actions 编。
3. 提交：`commit` → `git push`（SSH）。**不要**把 PAT 写进 remote URL / `.git/config`。
4. 构建失败时，等用户贴回 Actions 红色日志，据 `error Cxxxx` 自修。

## TODO（状态：2026-09-18）

- [x] **共享内存真实数据链路（代码层）**：`frida_host.py --shm` 把真实帧写入 `UcfFrame`，
      C++ 端 `SharedTransport` 读取；`tests/test_shm_bridge.py` 校验布局。**待实机验证（路线 B）**。
- [x] **透明窗口 flip / blt 双路径（VM 路线 A 已验证）**：Win11 虚拟机跑通合成源自演；
      真实帧下可见性待路线 B 确认。
- [x] **`overlay_viz` 血条 / 距离 / 骨骼**：绿-黄-红血条、距离过滤、19 槽 `BoneState` 骨骼绘制已实现。
- [x] **`smooth` 接到自有游戏相机（只读）**：FOV 内距屏心最近选靶 + 双选靶方案 + 取点配置；
      只读 yaw/pitch 输出；不写游戏内存/不 Hook（本地鼠标模拟模块见根 AGENTS.md §0.1，默认 OFF）。
- [~] **真实帧 fixtures 补全**：已有 `real_frame_level3.json` + `tests/test_projection_parity.py`
      （阈值 1e-5）；仍建议多场景补帧。
- [ ] **C++ 叠加层视口映射校准（真实游戏高优先）**：当前 C++ 用 `Frame.width/height`
      （游戏渲染分辨率，实测 800×600）直接投影到全屏 swapchain；若桌面/窗口分辨率不同，
      框会整体偏移。需参照 Python `compute_viewport` 的 letterbox/stretch 映射（见 `HOST_OVERLAY.md`「框位置对不上」）。
- [ ] **实机验证后的 bug 修复**：路线 B 跑通后据 `ucf_debug.log` + 截图自修
      （可能含分辨率映射、字体兜底、点击穿透在真实全屏下的表现）。
