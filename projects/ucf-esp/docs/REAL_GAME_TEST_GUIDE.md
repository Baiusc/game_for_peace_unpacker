# 实机测试行动指南：C++ 叠加层 × UnityCrossFire

> 目标：在 **Windows（Win11 真机 / 虚拟机）**里跑 `ucf_overlay_win32.exe`，验证 ESP / Aimbot（只读） / 菜单。
> 不涉及旧的 Python(tkinter) 叠加层。
> 适用：你拥有源码与授权的**单机**游戏 UnityCrossFire；不针对任何联机/第三方进程。

---

## 0. 当前状态（2026-09-18 更新）

- **shm 桥已代码层接通**：commit `cc2ccc2` 让 `frida_host.py --shm` 把真实帧写入 `UcfFrame`，
  C++ 端 `SharedTransport` 自动读取；`tests/test_shm_bridge.py` 校验二进制布局。
  ⇒ 路线 B 的"数据链路"代码已通，**只差你的实机验证**（真机/VM 跑一次确认画面正确）。
- **路线 A（无游戏烟雾测试）已在 VM 跑通**：透明窗口 + 中文菜单 + HOME/DELETE 分键 +
  点击穿透 + FOV 圆=选靶圆 + 选中高亮 + HUD 均验证过（合成源）。
- **真实数据链路"写端"只有 frida_host --shm**：`shm_writer.py` 仍仅合成源，用于"先合成对齐"。

---

## 1. 路线 A：无游戏烟雾测试（已验证，可复跑）

VM 里直接跑 `ucf_overlay_win32.exe`（无共享内存时自动 `SyntheticSource` 自演）。
检查项：中文菜单、HOME 切菜单 / DELETE 切 ESP、点击穿透、FOV 圆=选靶圆、选中高亮、
HUD 右下不重叠、配置写 `ucf_overlay.ini`。**局限：玩家是合成的**，只证渲染/菜单/选靶算法。

---

## 2. 路线 B：接真实游戏（你今晚实机做）

### 2.1 frida_host.py 仍必须跑；frida_probe.py 一般不用
- C++ 叠加层是**纯消费端**，只从 `"UcfFrame"` 读 `Frame`，自己不碰游戏进程、不注入 frida。
  所以**今天仍需要 `frida_host.py`**（注入+读游戏），只是它现在用 `--shm` 把帧写共享内存，
  而不是只喂 tkinter。
- `frida_probe.py` 是诊断"该注哪个进程"的辅助工具；昨天已跑通，**今晚不必每次跑**，
  只有当出现"IL2CPP module not loaded / 注进空壳"时才用 `--list` / `--probe` 排查。

### 2.2 准备「关闭反作弊」的游戏构建（必须）
内置 Anti-Cheat Toolkit（`InjectionDetector` / `WallHackDetector`）。
开发期用关闭反作弊的构建；否则 `--level 0` 闪退即被检测，**不要对抗**，回 Unity 工程关 Detector 重编。

### 2.3 VM / 真机启动顺序
```powershell
# 终端 1：启动/附加游戏 + 注入 frida + 把真实帧写进 "UcfFrame"（默认关 tkinter 层）
python tools\frida_host.py --game "C:\路径\UnityCrossFire.exe" --shm
#   进对局后按 INS 注入；Ctrl+C 退出时关闭映射（不删除共享内存对象）

# 终端 2：C++ 叠加层（检测到 "UcfFrame" 自动从合成源切到真实帧）
ucf_overlay_win32.exe
```

### 2.4 验证清单
**ESP（真实）**：框跟随真实玩家（本地绿/队友蓝/敌人红）；血条、距离正确；骨骼（若数据就绪）画出；
框位置与游戏内物体对得上（若游戏渲染分辨率 ≠ 桌面分辨率可能偏移，见 §3 隐患）。
**Aimbot（只读）**：菜单开「显示 FOV / 目标选择」；FOV 圆内距屏心最近者高亮；只读 yaw/pitch 显示；
不写游戏内存/不 Hook（本地鼠标模拟按根 AGENTS.md §0.1 授权，默认 OFF）。
**菜单**：所有开关/滑块在真实数据流下可用；中文、热键、点击穿透、配置保存正常。

### 2.5 已知隐患 / 待校准（实测时留意）
- **分辨率映射（最高优先）**：C++ 叠加层用 `Frame.width/height`（游戏渲染分辨率，实测 800×600）
  投影到全屏 swapchain。若桌面分辨率 ≠ 游戏分辨率，框可能整体偏移/缩放。
  第一轮建议**窗口化且游戏分辨率=桌面分辨率**；偏移校准见下方 §4 与 `HOST_OVERLAY.md`「框位置对不上」。
- **字体**：VM/真机若缺 `msyh.ttc` 等，菜单退回 ASCII 兜底。
- **调试日志**：exe 同目录 `ucf_debug.log`（flip/BLT、present 返回值、每 120 帧心跳）；
  菜单点不中或收不到数据时，把现象 + 该 log 发回分析。
- **SharedTransport 未跨进程实跑过**：Linux ctest 只覆盖 `LocalTransport` 进程内双缓冲；
  真实跨进程可能有对齐/字节序/槽语义细节 bug——所以务必先做 §4 的「先合成、后真实」。

---

## 3. 注入 / 反作弊排查速查（游戏侧，非叠加层）

| 现象 | 含义 | 处理 |
|---|---|---|
| `--level 0` 就闪退 | 注入本身被检测（ACTK `InjectionDetector`） | 换关闭反作弊的构建 |
| 注入前不注入也闪退 | 游戏自身问题 | 与本工具无关 |
| `--level 2` 活、`--level 3` 崩 | 崩在取单例/遍历玩家/读坐标 | 看 `Player.log`，`--probe` 定位 |
| `Player.log` 干净、进程静默消失 | 外部终止（反作弊/ taskkill） | 关反作弊 |

`Player.log`：`%LOCALAPPDATA%\..\LocalLow\Alexander_GaGa\UnityCrossFire\Player.log`
详细二分法见 `docs/HOST_OVERLAY.md` 的 `--level` / `--probe`。

---

## 4. 数据契约对齐校验（关键，避免读到乱帧）

C++ 读到的字节必须和 Python 写出的**逐字节一致**。

**两段验证法（强烈建议）：**
1. **先合成对齐**：VM 上 `python cpp_overlay\tools\shm_writer.py`（合成源）+ `ucf_overlay_win32.exe`，
   确认 ESP 框出现且坐标合理（合成源坐标已知），证跨语言布局对齐。
2. **后换真实**：再 `frida_host.py --shm` 的真实帧。

对齐要点：Python `shm_writer.FRAME_FMT` / `SLOT_FMT`（小端、4 字节对齐）<==> C++ `shared_state.cpp` 的
`FrameSlots`（`int cur + Frame`）。改任一边都要同步并跑 cpp_overlay ctest 传输用例。

---

## 5. 给 Codex 的提示词（剩余工作，非"接桥"——桥已完成）

> 下面三段直接发给 Codex（或任一 coding agent）。**只动 cpp_overlay/ 与 tests/，不碰 frida/游戏进程。
> Aimbot 角度计算与选靶保持纯计算（不写游戏内存/不 Hook）；本地鼠标模拟（SendInput/mouse_event）按根 AGENTS.md §0.1 授权，
> 实现为独立 `input_sim/` 模块、菜单开关、**默认 OFF**，不混入纯计算模块**。

### 5.1 高优先：C++ 叠加层视口映射校准（真实分辨率偏移）
```
任务：让 C++ 叠加层的 ESP 框在「游戏渲染分辨率 ≠ 桌面分辨率」时仍对齐游戏物体。
仓库：game_for_peace_unpacker / projects/ucf-esp/cpp_overlay
背景：overlay_win32 project_frame() 当前用 Frame.width/height（游戏内渲染分辨率，实测 800x600）
      直接投影到全屏 D3D11 swapchain；桌面分辨率不同时框整体偏移/缩放。
参考：Python 侧 tools/overlay_tk.py 的 compute_viewport() 已实现 letterbox/stretch 映射
      （见 docs/HOST_OVERLAY.md「框位置对不上」）：按游戏窗口客户区算映射，
      auto=宽高比一致走 stretch、否则 letterbox 等比居中（保留黑边）。
要求：
1. 在 overlay_win32 / overlay_viz 的 build_draw_list 视口映射处，引入与 Python 等价的映射：
   已知 Frame.width/height 与 swapchain 的 cw/ch，计算 scale 与 offset（letterbox 等比居中）。
   所有 ScreenMark 的屏幕坐标乘以 scale 再加 offset，使 ESP 框贴合游戏内物体。
2. 菜单增加"overlay-fit"选项（auto/stretch/letterbox），默认 auto；可加 overlay-rect 手动指定。
3. 不得破坏 NDC 裁剪、不得改投影数学（world_to_screen 保持原样，只改"屏幕坐标→swapchain 像素"的映射）。
4. 单测：在 tests/ 加一个纯函数映射测试（compute_viewport 等价逻辑），覆盖 800x600→2880x2160 letterbox、
   auto 阈值、强制 stretch 三种情形；Linux 可跑。
验证：VM 上游戏窗口化且分辨率≠桌面，框与物体对齐；单测通过；Linux ctest 全过。
```

### 5.2 实机反馈后的 bug 修复（通用模板）
```
任务：根据用户实机跑 C++ 叠加层的反馈修复问题（ESP/菜单/选靶/透明窗口相关）。
仓库：game_for_peace_unpacker / projects/ucf-esp/cpp_overlay
输入：用户会提供 (a) ucf_debug.log 日志片段 (b) 现象描述与截图 (c) 复现步骤。
边界：仅用于自研单机游戏；Aimbot 角度/选靶纯计算（不写游戏内存/不 Hook）；不做任何反作弊对抗。
      本地鼠标模拟按根 AGENTS.md §0.1 授权（独立 input_sim/ 模块、默认 OFF），不在本节默认启用。
步骤：
1. 先读 docs/CPP_OVERLAY.md（透明窗口/flip-blt/点击穿透/中文字体）与 docs/PITFALLS.md（跨模块踩坑表）定位已知坑。
2. 若涉及"框位置偏移"→ 优先看 5.1 的视口映射是否未做；
   若"菜单点不中"→ 看 update_click_through() 的菜单矩形轮询逻辑；
   若"全屏黑/看不见"→ 看 init 日志 used=/clear= 判定 flip/blt 路径；
   若"中文 ???? "→ 看 init_overlay_fonts() 字体候选。
3. 改后必须：Linux 跑 cpp_overlay ctest 全过；Windows 渲染层改动走 Actions 重编（只改 cpp_overlay/** 触发）。
4. 不擅自扩大范围；改动同步到对应 deep-doc 的「排错」并追加 PITFALLS.md 第二节。
```

### 5.3 可选增强：框标签中文名 + 框随距离缩放（契约同步）
```
任务：增强 ESP 绘制（可选，非阻塞）。
仓库：projects/ucf-esp/cpp_overlay
1. 框标签显示玩家名：PlayerState.name[32] 已有字段；overlay_viz 当前用 ASCII 的 kind/hp/距离标签
   是为保 overlay_tests 对标签格式的断言。若要显示中文名，需同步放宽/更新 tests 的标签断言，
   并确认 name 在 frida_host --shm 的真实帧里被正确填充（查 shm_writer._ps_items 的 name 打包）。
2. 框随距离缩放：当前框固定尺寸（overlay_tests 断言 boxes[0].w==40）。若要近大远小，
   需先改 tests/overlay_tests.cpp 的断言再改 overlay_viz 的尺寸计算，保证 ctest 仍过。
要求：两项都需"先改测试契约、再改实现"，并跑 Linux ctest 确认无回归。Aimbot 仍只读。
```

### 5.4 可选：真实帧 fixtures 补全
```
任务：扩充真实帧回归样本。
仓库：projects/ucf-esp
做法：用 frida_host.py --dump-frame DIR --game <exe> --shm 在真实对局里落盘原始 Frame JSONL；
      挑有代表性场景（多队友/多敌人/不同距离/骨骼齐全）的帧，转成 tests/fixtures/real_frame_*.json；
      在 tests/test_projection_parity.py 中增加用例（阈值 1e-5，与 C++ world_to_screen 对齐）。
注意：落盘的是原始 Frame，不是投影后的 marks；写临时文件后原子替换，避免半个 JSON。
```

---

## 6. 你现在（人）的最小行动清单

1. 下载最新 `ucf_overlay_win32.exe`（Actions 已含 --shm 链路）。
2. **路线 A 复跑**（VM/真机均可，无需游戏）：核对 §1 清单，确认合成源下一切正常。
3. **路线 B 实机**（今晚 Win11 真机）：按 §2.3 两条命令，重点核对 §4「先合成、后真实」，
   并盯 §2.5 的分辨率映射隐患。
4. 若框偏移 → 把现象+`ucf_debug.log` 发回，按 §5.2 让 Codex 修（很可能就是 §5.1 的视口映射未做）。
5. 游戏务必用**关闭反作弊**构建；`--level 0` 闪退即反作弊还在。
