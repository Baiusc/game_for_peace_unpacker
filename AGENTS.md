# AGENTS.md — 多 Agent 协作入口（game_for_peace_unpacker）

> 给 **CodeBuddy / CODEX / 豆包 / WorkBuddy** 等所有 agent 的统一说明。
> **动手前先读第 0 节（边界），再读第 4 节（该读哪个文档）。**

---

## 0. 边界与合规（最高优先级，所有 agent 必须遵守）

- 本仓库是**个人学习实验室**：作者自研**单机游戏 UnityCrossFire**（Unity IL2CPP，32-bit），
  用于练习 IL2CPP 逆向、内存读取、投影计算、叠加层绘制。
- 仅用于**自己拥有源码的单机游戏**；不联机、不涉及其他玩家、不盈利、不买卖、不分发。
- **禁止**针对任何在线多人游戏（CS2 / Valorant / DeltaForce / 等）做内存读写、注入、Hook、
  Aimbot / ESP；
- `projects/ucf-esp` 下所有工具都是给「自己的游戏」用的**调试可视化**，不是绕过他人保护。
- 涉及反作弊（游戏内置 Anti-Cheat Toolkit）：开发期用**关闭反作弊**的构建调试；
  `--level 0` 就闪退即可确认是注入被检测，**不要去对抗**。

### 0.1 明确允许（自研单机限定）

在以下**全部**前提下，允许实现「本地输入模拟」模块：
- 目标是**自研单机游戏 UnityCrossFire**（作者拥有全部源码）；
- 运行在**作者自己的机器**上，不联机、不涉及其他玩家；
- 使用**关闭反作弊的调试构建**；
- 模拟输入**只作用于本机前台窗口**，不针对任何第三方进程；
- 不注入、不 Hook、不写游戏内存，只通过 Win32 公开 API（`SendInput` / `mouse_event`）产生本地鼠标事件。

**明确禁止**（不因上一条放松）：
- 对任何在线多人游戏做同类操作；
- 对抗 / 绕过任何反作弊系统；
- 把输入模拟模块分发给第三方。

> **「不注入」的本意**是「不写游戏内存、不 Hook、不注入到游戏进程」，
> **不指**「不能模拟本地鼠标事件」。凡符合本节的本地模拟输入，必须实现为
> `cpp_overlay/src/input_sim/` 独立模块、菜单显式开关、**默认 OFF**，
> 不混入 `smooth.cpp` / `select_target()` 等纯计算模块。

---

## 1. 仓库地图

| 路径 | 主题 | 运行环境 |
|---|---|---|
| `projects/ucf-esp/` | **本项目核心**：UnityCrossFire 调试工具（Python+tkinter 运行时叠加层 + C++ D3D11 叠加层骨架） | Windows（核心库 Linux 可测） |
| `projects/ue_pak_archive_lab/` | UE PAK 文件格式与归档读写 | — |
| `projects/network_auth_audit_lab/` | 应用层协议兼容性与健壮性 | Python |
| `projects/kernel_memory_defense_lab/` | Linux/Windows 内核行为防御原型 | Python |
| `3rd_project/` | **只读参考**：公开逆向学习项目；仅借鉴通用技术点，**不复制项目特定逻辑、不用于他人系统** | — |

> 其它三个 lab 与本 ESP 项目**无依赖**。要改 `ucf-esp` 直接看 `projects/ucf-esp/AGENTS.md`。

---

## 2. 关键数据流（ucf-esp）

```
UnityCrossFire.exe (GameAssembly)
   │ frida-il2cpp-bridge 读 相机矩阵 / 玩家坐标 / 血量 (tools/frida_dump.js)
   ▼
frida_host.py ──投影(esp_core + combine_pv / world_to_screen)──► Frame (列主序 w2c/proj + players)
   │ 实时：tools/overlay_tk.py 画 (Python / tkinter)
   │ 或：写入命名共享内存 "UcfFrame" (tools/shm_writer.py)
   ▼
cpp_overlay (ucf_overlay_win32.exe) 读共享内存 ─► 投影+可视化 ─► D3D11 + ImGui 透明叠加层
```

数据契约（`Frame` / `PlayerState`，列主序矩阵）见 `projects/ucf-esp/cpp_overlay/README.md`
与 `projects/ucf-esp/docs/STATIC_REFERENCE.md`。Python 宿主与 C++ 叠加层共用同一契约，
投影结果须一致。

---

## 3. 构建 / CI / 推送（agent 须知）

- **核心库（纯 C++，平台无关）**：可在 Linux 构建并跑 ctest：
  `cd projects/ucf-esp/cpp_overlay && cmake -S . -B build && cmake --build build && ctest --test-dir build`。
- **Windows 叠加层 exe**：由 GitHub Actions `build-ucf-overlay.yml` 编译（MSVC `/MT` 静态链接），
  产物 `ucf_overlay_win32.exe` 作为 artifact 上传，直接拷到 Win11 虚拟机运行，无需装 VS / SDK / VC++ 运行库。
  ⚠️ workflow 只对 `projects/ucf-esp/cpp_overlay/**` 的改动触发，改其它目录不会重编 exe。
- **推送方式（与用户约定）**：push 走 **SSH key**（remote `github` = `git@github.com:Baiusc/game_for_peace_unpacker.git`）。

- Agent 改动代码后的闭环：`edit → commit → git push (SSH)` → 轮询 `actions/runs?head_sha=<sha>` 等构建；
  `success` 告知用户去下 exe 测；`failure` 请用户贴红色日志，据 `error Cxxxx` 自修。

---

## 4. 文档索引（按 agent 角色读）

| 想做的事 | 读 |
|---|---|
| 上手整个项目 / 合规边界 | 本文件（AGENTS.md） |
| ucf-esp 子项目结构、构建、TODO | `projects/ucf-esp/AGENTS.md` |
| C++ D3D11 叠加层架构与**透明窗口排错** | `projects/ucf-esp/docs/CPP_OVERLAY.md` |
| Python 宿主 / tkinter 叠加层排错 | `projects/ucf-esp/docs/HOST_OVERLAY.md` |
| IL2CPP 偏移 / 字段 / ObscuredInt 解密 | `projects/ucf-esp/docs/STATIC_REFERENCE.md` |
| Ghidra 静态初筛 | `projects/ucf-esp/docs/STATIC_TRIAGE.md` |
| **跨模块踩坑总表（不要重犯）** | `projects/ucf-esp/docs/PITFALLS.md` |

---

## 5. 协作约定（多 agent）

- 每个 agent 先读对应 AGENTS.md 与本文件第 0 节，再改代码。
- **踩坑、关键修改、决策必须追加到 `docs/PITFALLS.md`**（见其模板），并在对应 deep-doc 的「排错」节补细节。
- 改动共享数据契约（`Frame` / `PlayerState`）时，必须同步更新：
  `shared_state.hpp` + `esp_core.py` + `frida_dump.js` + `STATIC_REFERENCE.md`，
  并跑 `tests/test_dump_contract.py` + cpp_overlay ctest。
- Commit message 用中文简述「做了什么 + 为什么」，便于人类与其它 agent 追述。

---

## 6. TODO（ucf-esp）

- [ ] C++ 叠加层接入真实共享内存（已支持 `SharedTransport("UcfFrame")`，待与 frida_host 打通联调）。
- [ ] 透明窗口 flip / blt 双路径在虚拟机实测确认（见 `docs/CPP_OVERLAY.md`）。
- [ ] 血条 / 距离标注 / 骨骼包围盒（`overlay_viz` 已留接口）。
- [ ] 鼠标平滑（`smooth.cpp`）目前只算角度（不写游戏内存/不 Hook；本地输入模拟见 §0.1），待接到自有游戏相机。
- [ ] 真实帧回归样本补全（`tests/fixtures/real_frame_*.json`）。
