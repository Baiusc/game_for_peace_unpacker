# ucf-esp 子项目 — Agent 指南

> 本文件是 CodeBuddy / CODEX / 豆包 / WorkBuddy 进入 `projects/ucf-esp` 后的**必读**。
> 根级约定（边界 / 合规 / 推送流程）见仓库根 `AGENTS.md`。

## 这是什么

你自己的**单机游戏 UnityCrossFire**（Unity IL2CPP，32-bit）的调试工具集合。两种运行时叠加层实现：

1. **Python + tkinter**（`tools/overlay_tk.py` + `tools/frida_host.py`）：已可用，
   含 DPI 感知、坐标映射(stretch/letterbox)、失焦隐藏、点击穿透、防闪烁、跨线程队列。
2. **C++ D3D11 + ImGui**（`cpp_overlay/`）：性能 / 功能增强骨架，
   GPU 渲染 + 共享内存数据链路 + 菜单 / 配置 + 平滑算法。

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
| `src/smooth.hpp/.cpp` | 相机角度指数平滑 + 目标选择（**只算角度，不注入鼠标**） | 核心 |
| `src/config.hpp/.cpp` | key=value 配置持久化 | 核心 |
| `src/overlay_win32.*` | Frame→屏幕标记投影（复刻 `esp_core`）+ 合成数据源 `SyntheticSource` + `render_draw_list` 画到 ImGui 背景层 | WIN32 |
| `src/menu_win32.*` | ImGui 菜单（INSERT 切换） | WIN32 |
| `src/main_win32.cpp` | 分层透明窗口 + D3D11 + ImGui 主循环 | WIN32 |

构建 / 运行 / 数据契约见 `cpp_overlay/README.md`；
**透明窗口与 flip/blt 踩坑见 `docs/CPP_OVERLAY.md`**。

## 关键步骤（agent 改代码时）

1. 改数据契约 → 同步 `shared_state.hpp` + `esp_core.py` + `frida_dump.js` + `STATIC_REFERENCE.md`，
   跑 `tests/test_dump_contract.py` 与 cpp_overlay ctest。
2. 改 C++ 逻辑 → 本地 `cmake -S . -B build && ctest`（Linux 可验证核心库）；Windows 渲染层靠 Actions 编。
3. 提交：`commit` → `git push`（SSH）。**不要**把 PAT 写进 remote URL / `.git/config`。
4. 构建失败时，等用户贴回 Actions 红色日志，据 `error Cxxxx` 自修。

## TODO

- [ ] 共享内存与 frida_host 联调（真实数据打通）。
- [ ] 透明窗口 flip / blt 双路径在虚拟机实测确认。
- [ ] 完善 `overlay_viz`（血条 / 距离 / 骨骼包围盒）。
- [ ] `smooth` 接到自有游戏相机（仅算角度，不注入）。
- [ ] 真实帧 fixtures 补全（`tests/fixtures/real_frame_*.json`）。
