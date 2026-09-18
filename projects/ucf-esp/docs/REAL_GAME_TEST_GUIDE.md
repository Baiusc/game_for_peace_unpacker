# 实机测试行动指南：C++ 叠加层 × UnityCrossFire

> 目标：在 **Windows 虚拟机**里实机跑 `ucf_overlay_win32.exe`，验证 ESP / Aimbot（只读） / 菜单。
> 不涉及旧的 Python(tkinter) 叠加层。
> 适用范围：你拥有源码与授权的**单机**游戏 UnityCrossFire；不针对任何联机/第三方进程。

---

## 0. 重要前提与现状（先读，避免白跑）

1. **实机环境 = Windows（Win11 虚拟机）**。`ucf_overlay_win32.exe` 是 WIN32/D3D11 程序，
   Linux 只能跑核心库 ctest（投影/选靶/传输单测），**不能渲染叠加层**。
2. **真实数据链路当前尚未打通**（关键，决定路线）：
   - 读端：`cpp_overlay/src/shared_state.cpp` 的 `SharedTransport("UcfFrame")` 已就绪；
   - 写端：`cpp_overlay/tools/shm_writer.py` **只有合成数据源**（绕相机转的假玩家），不读游戏；
   - `tools/frida_host.py` 只把帧推给 Python(tkinter) 叠加层，**没有写共享内存的路径**。
   - 结论：**现在没有任何程序把真实游戏帧写进 `"UcfFrame"`**。
3. 因此分两条路线（见 §1 / §2）。**§1 现在就能跑**（验证菜单/渲染/选靶逻辑，玩家是假的）；
   **§2 需要 Codex 先接好 shm 桥**（见 §5 提示词）才能看真实玩家。

---

## 1. 路线 A：无游戏烟雾测试（立刻，VM 内）

验证透明窗口 + ImGui 菜单 + ESP 渲染 + 选靶逻辑是否正常工作（用内置合成源自演）。

### 步骤
1. 把 Actions 编出的 `ucf_overlay_win32.exe`（静态链接 `/MT`，VM 无需装 VS/VC++ 运行库）
   拷到 Win11 虚拟机任意目录。
2. 双击运行，或 `cmd` 里 `ucf_overlay_win32.exe`。
3. 默认行为：菜单**显示**、ESP **隐藏**；无共享内存时自动退回 `SyntheticSource` 自演。

### 预期与检查项（逐项对）
- [ ] **中文正常**：菜单项显示中文（msyh.ttc 等候选加载失败则退回 ASCII 兜底）。
- [ ] **HOME 切菜单**：按 HOME 显/隐菜单；**DELETE 切 ESP 绘制层**。
- [ ] **点击穿透**：鼠标悬停菜单区 → 菜单可点/可拖/滑块可用；
      鼠标在菜单外 → 点击穿透到桌面/游戏，不拦操作。
- [ ] **ESP 框出现**（按 DELETE 后）：合成玩家绕相机转，框跟随；
      本地绿 / 队友蓝 / 敌人红（颜色可在菜单调色板改）。
- [ ] **FOV 圆 = 选靶圆**：菜单里拖 `FOV` 滑块，屏幕上画的 FOV 圈半径与选靶半径同公式
      （`main_win32.cpp` 的 `fov_radius = (fov/90)*min(w,h)*0.5`）。
- [ ] **选靶高亮**：在 FOV 圆内、距屏幕中心最近的合成玩家被标记为当前目标（Aimbot 只读可视化）。
- [ ] **HUD 不重叠**：诊断行在右下角，不与菜单重叠；有 `FPS:xx.x`。
- [ ] **配置持久化**：改开关/颜色后退出（END 或"退出程序"按钮），`ucf_overlay.ini` 被写出；
      重开 exe 设置保留。

### 局限
玩家坐标是合成源，**不是真实游戏数据**。只能证明"渲染/菜单/选靶算法"通了。

---

## 2. 路线 B：接真实游戏数据（需先打通 shm 桥）

### 2.1 先让 Codex 接桥（见 §5 提示词）
让 `frida_host.py` 在收到真实帧后，把 `w2c/proj/local/players` 用
`shm_writer.build_frame` 的**同一二进制布局**写进命名共享内存 `"UcfFrame"`
（双缓冲：写后台槽→翻转 `cur`）。C++ 端 `SharedTransport` 自动读取并切到真实帧。

### 2.2 准备「关闭反作弊」的游戏构建（必须）
- 你游戏内置 Anti-Cheat Toolkit（`InjectionDetector` / `WallHackDetector`）。
- 开发期请用**关闭反作弊**的构建：在 Unity 工程里关掉这两个 Detector，
  或用 `#if !DEVELOPMENT_BUILD` 包住初始化后重新构建，把 exe 拷到 VM。
- 否则 Frida 注入会被检测误报（闪退/弹窗），与本工具无关。

### 2.3 VM 启动顺序
```powershell
# 终端 1：启动游戏并注入 frida（注入后把真实帧写进 "UcfFrame"）
python tools\frida_host.py --game "C:\路径\UnityCrossFire.exe" --shm
#   （先正常启动游戏；进入对局后按 INS 注入；Ctrl+C 退出时关闭共享内存映射）

# 终端 2：运行 C++ 叠加层（检测到 "UcfFrame" 即自动从合成源切到真实帧）
ucf_overlay_win32.exe
```
> 注：`--shm` 是 Codex 要新增的开关（见 §5）。在此之前，`shm_writer.py` 只是合成源，
> 用它跑 C++ 叠层只能验证「Python↔C++ 二进制布局对齐」，看不到真实玩家。

### 2.4 验证清单
**ESP（真实）**
- [ ] 框跟随真实玩家；本地绿/队友蓝/敌人红正确。
- [ ] 血条（`hp/maxHp`）、距离（`|local.pos - p.pos|`）正确显示。
- [ ] 骨骼（若 `bones` 数据就绪）画出骨架。
- [ ] 框位置与游戏内物体对得上（若游戏渲染分辨率 ≠ 桌面分辨率，可能有偏移，见 §3 隐患）。

**Aimbot（只读，不注入）**
- [ ] 菜单开「显示 FOV / 目标选择」。
- [ ] FOV 圆内、距屏幕中心最近的目标被高亮为当前目标。
- [ ] 选中后只读 yaw/pitch 计算并在 HUD/调试信息显示；**不移动鼠标、不注入输入**。

**菜单（真实帧下）**
- [ ] 所有开关/滑块在真实数据流下仍可用。
- [ ] 中文、热键、点击穿透、配置保存全部正常。

### 2.5 已知隐患 / 待校准（实测时留意）
- **分辨率映射**：C++ 叠加层是全屏透明窗口，投影用 `Frame.width/height`
  （你游戏实测 800×600）。若桌面分辨率 ≠ 游戏分辨率，框可能整体偏移/缩放。
  建议先**窗口化、且游戏分辨率设为与桌面一致**跑第一轮；偏移校准是后续活。
- **字体**：VM 若缺 `msyh.ttc/msyh.ttf/simhei.ttf/simsun.ttc`，菜单退回 ASCII 兜底。
- **调试日志**：exe 同目录会写 `ucf_debug.log`（flip/BLT、present 返回值、每 120 帧心跳）；
  菜单点不中或收不到数据时，把现象 + 该 log 发回分析。
- **SharedTransport 未跨进程实测**：`SharedTransport` 只在 Windows 编译，Linux ctest 只覆盖
  `LocalTransport` 进程内双缓冲。真实跨进程可能有对齐/字节序/槽语义细节 bug——
  所以务必先做 §4 的「先合成、后真实」两段校验。

---

## 3. 注入 / 反作弊排查速查（游戏侧，非叠加层）

| 现象 | 含义 | 处理 |
|---|---|---|
| `--level 0` 就闪退 | 注入本身被检测（ACTK `InjectionDetector`） | **不要对抗**；换关闭反作弊的构建 |
| 注入前不注入也闪退 | 游戏自身问题 | 与本工具无关 |
| `--level 2` 活、`--level 3` 崩 | 崩在取单例/遍历玩家/读坐标 | 看 `Player.log`，按 `--probe` 定位 |
| `Player.log` 干净、进程静默消失 | 外部终止（反作弊 `Application.Quit`/taskkill） | 关反作弊 |

`Player.log` 路径：`%LOCALAPPDATA%\..\LocalLow\Alexander_GaGa\UnityCrossFire\Player.log`
详细二分法见 `docs/HOST_OVERLAY.md` 的 `--level` / `--probe` 两节。

---

## 4. 数据契约对齐校验（关键，避免读到乱帧）

C++ 读到的字节必须和 Python 写出的**逐字节一致**，否则框会乱飞或崩溃。

**两段验证法（强烈建议）：**
1. **先合成对齐**：VM 上跑 `shm_writer.py`（合成源，自带）+ `ucf_overlay_win32.exe`，
   确认 ESP 框出现且坐标合理（合成源坐标已知），证明二进制布局跨语言对齐。
2. **后换真实**：再切到 `frida_host.py --shm` 的真实帧。

对齐要点：
- Python 侧布局：`cpp_overlay/tools/shm_writer.py` 的 `FRAME_FMT` / `SLOT_FMT`
  （`"<i" + Frame`，小端、4 字节对齐）。
- C++ 侧布局：`cpp_overlay/src/shared_state.cpp` 的 `FrameSlots`
  （`int cur + Frame`，列主序矩阵）。
- 两者必须一致；改任一边的结构体都要同步另一边，并跑 `cpp_overlay` ctest 的传输用例。

---

## 5. 给 Codex 的提示词（接 shm 桥，打通真实数据）

> 把下面整段直接发给 Codex（或任何 coding agent）。只动 `frida_host.py`，
> 复用 `shm_writer.py` 的二进制布局，**不要改 C++ 侧契约**。

```
# 任务：把 frida_host.py 读到的真实游戏帧写进命名共享内存 "UcfFrame"，
#       让 cpp_overlay 的 SharedTransport 自动读取（打通真实数据链路）。
# 仓库：game_for_peace_unpacker / projects/ucf-esp
# 边界：仅用于作者自研单机游戏 UnityCrossFire；不针对任何联机/第三方进程；
#       本仓库 Aimbot 只做目标选择+角度计算+可视化，不注入鼠标/输入。

## 背景（必读）
- cpp_overlay 已通过 SharedTransport("UcfFrame") 从 Windows 命名共享内存读帧。
- 但写端 cpp_overlay/tools/shm_writer.py 只有合成数据源（不读游戏）；
- tools/frida_host.py 只把帧推给 Python(tkinter) 叠加层，没有写共享内存的路径。
- 需要新增一条「frida 真实帧 → "UcfFrame" 共享内存」的链路。

## 二进制布局（严格复用，不得自创）
- 复用 cpp_overlay/tools/shm_writer.py 里的 build_frame() / FRAME_FMT / SLOT_FMT /
  PS_FMT / BONE_FMT / N_PLAYERS / N_BONES / SHM_NAME="UcfFrame" / SHM_SIZE。
- 帧结构：int cur(4B) + Frame；Frame = w2c[16]f + proj[16]f + width i + height i
  + inGame ? + 3x + local PlayerState + players[64] PlayerState + playerCount i。
  PlayerState = pos[3]f + team i + hp i + maxHp i + isDead ? + name[32]s + 3x
  + bones[19](pos[3]f + valid ? + 3x)。全部小端 <、4 字节对齐。
- C++ 侧对应 cpp_overlay/src/shared_state.cpp 的 FrameSlots，必须逐字节一致。

## 要做的改动（additive，不破坏现有行为）
1. 给 tools/frida_host.py 增加 `--shm` 开关（默认关，保持现有 tkinter 叠加层路径不变）。
2. 当 --shm 开启时：在 on_message 收到 frida 帧（已含 w2c[16]/proj[16]/local/players，
   以及 Screen 的 width/height）后，调用 shm_writer 的 build_frame() 打包成 bytes，
   用 mmap(-1, SHM_SIZE, tagname="UcfFrame") 打开（不存在则创建），
   写入后台槽（slot = 1 - cur）后翻转 cur（与 shm_writer 的双缓冲语义一致）。
3. 把 build_frame / FRAME_FMT 等抽取到一个双方 import 的共享模块
   （如 tools/shm_frame.py），shm_writer.py 与 frida_host.py 都从它 import，
   避免两份布局漂移。
4. 退出（Ctrl+C）时关闭 mmap 映射、释放资源。共享内存对象本身不主动删除
   （与 cpp_overlay 注释一致：映射始终关闭，不执行删除）。
5. frida_host 已有的 --no-overlay / --level / --interval / --probe 等参数全部保留。

## 验证（在 Windows VM 上）
- 单元层：在 Linux 也能跑一个纯布局断言（import tools.shm_frame，打包一个已知帧，
  断言 len == SHM_SIZE、与 shared_state.cpp 的 FrameSlots 大小一致）。
- 集成层（VM）：
  a. 先 `python cpp_overlay/tools/shm_writer.py`（合成源）+ ucf_overlay_win32.exe，
     确认 C++ 框出现且坐标合理（证明跨语言布局对齐）。
  b. 再 `python tools/frida_host.py --game <path> --shm`，进入对局按 INS 注入，
     确认 C++ 叠加层从合成源切到真实帧、框跟随真实玩家。
- 若 C++ 收不到数据：检查 "UcfFrame" 是否被创建、SHM_SIZE 是否 4+Frame、
  字节序/对齐是否与 FrameSlots 一致（用 a 步的合成对齐先排查）。

## 不要做
- 不要修改 cpp_overlay/src 任何文件（契约已定）。
- 不要引入新的网络/文件传输，只用 "UcfFrame" 命名共享内存。
- 不要把 frida agent 注入任何非自有进程。

## 交付
- 改完跑：仓库根 `python projects/ucf-esp/tests/test_dump_contract.py` 仍过；
  新增/复用的布局断言通过。
- 给出 VM 上验证 a/b 两步的结果截图或日志摘要。
```

---

## 6. 你现在（人）的最小行动清单

1. 下载最新 `ucf_overlay_win32.exe`（Actions run #11+，已含中文/分键/穿透/选靶）。
2. **路线 A**（无需游戏）：VM 里跑 exe，按 §1 清单逐项验证。把现象/截图发回。
3. 若路线 A 有问题（尤其点击穿透、中文、FOV圆≠选靶圆）→ 发 `ucf_debug.log` + 截图。
4. 想看真实玩家 → 先把 §5 提示词发给 Codex 接 shm 桥；桥通后再按 §2 跑真实对局，
   重点核对 §4 的「先合成、后真实」两段校验。
5. 真实游戏务必用**关闭反作弊**的构建；`--level 0` 闪退即说明反作弊还在。
