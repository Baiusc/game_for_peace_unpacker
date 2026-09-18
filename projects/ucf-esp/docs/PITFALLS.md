# 踩坑记录 / 关键修改总表（ucf-esp）

> **给所有 agent + 人类开发者。** 新增踩坑**追加到第二节表格**，并在对应 deep-doc 的「排错」补细节。
> 原则：先查这张表再动手，不重复踩。
> 详细实证见：`HOST_OVERLAY.md`（Python 宿主）、`CPP_OVERLAY.md`（C++ 叠加层）、
> `STATIC_REFERENCE.md`（IL2CPP 偏移）、`STATIC_TRIAGE.md`（Ghidra）。

---

## 一、关键修改记录（按时间，新→旧）

| 日期 | 改动 | 原因 | 详见 |
| --- | --- | --- | --- |
| 2026-09-18 | UI 修复：菜单 HOME / ESP 绘制层 DELETE（旧 ini 的 INSERT 自动迁移）；点击穿透动态开关；移除诊断红框/全屏 HUD（收进菜单）；菜单标签 ASCII 化 | INSERT 与注入键冲突；`WS_EX_TRANSPARENT` 常开导致菜单点不中；默认字体无 CJK 字形 | `CPP_OVERLAY.md` §4⑤/⑥ |
| 2026-09-18 | C++ 透明窗口：去掉 `WS_EX_LAYERED`，flip+PREMULTIPLIED / BLT+colorkey 兜底；加诊断红框+HUD+present_hr 日志 | `WS_EX_LAYERED` 让窗口整体不可见且阻断 flip（0x887A0001） | `CPP_OVERLAY.md` §3/§4① |
| 2026-09-18 | `scale=inf` 修复：视口缩放分母兜底 1280x720 + 校验 frame 尺寸 | 无共享内存时 frame 尺寸非法 → 分母 0 | `CPP_OVERLAY.md` §4② |
| 2026-09-17 | 投影加 NDC 裁剪（`NDC_CLIP=3.0`）；`world_to_screen` 保留原始值 | 本地玩家压相机头，透视除 w 极小，屏幕坐标放大几十倍 | `HOST_OVERLAY.md` 「屏幕坐标离谱」 |
| 2026-09-17 | `get_isDead` 改由血量推导 + 取帧调度到 Unity 主线程 | frida 线程调有方法体方法 → AV，进程静默消失 | `HOST_OVERLAY.md` 「注入后游戏消失」 |
| 2026-09-17 | 取单例改读静态 `backing field` / `gc.choose`，关闭 `get_instance()` | 泛型基类共享泛型方法带隐藏 `MethodInfo*` 参数 → AV | `STATIC_REFERENCE.md` §3 |
| 2026-09-17 | 矩阵/坐标走 `*_Injected(out)` + `ObscuredInt` 裸内存 XOR 解密 | 32 位下按值返回结构体依赖 frida ABI，易翻车 | `STATIC_REFERENCE.md` §1/§5 |
| 2026-09-17 | 宿主 `on_message` 兼容 dict/str/bytes，解析错误只报一次 | frida 16+ 把 `send(对象)` 解成 dict，`json.loads` 每帧刷屏 | `HOST_OVERLAY.md` `payload parse error` |
| 2026-09-17 | 叠加层 DPI 感知（per-monitor-v2）+ letterbox 映射 + 原生 topmost + 排除自身前台 + 队列 | 框位置偏 / 画到别的窗口 / 闪烁 / 点不动 | `HOST_OVERLAY.md` 「框位置对不上」 |

> 早期 commit 哈希以仓库 `git log` 为准；本表按改动性质记录，定位细节看「详见」列。

---

## 二、跨模块踩坑速查表

> 列：**现象 → 根因 → 修法 → 详细文档**

### Python 宿主 / tkinter 叠加层（`tools/`，详见 `HOST_OVERLAY.md`）
| 现象 | 根因 | 修法 |
| --- | --- | --- |
| `IL2CPP module not loaded` | 注进了同名空壳进程（反复运行留存根） | 启动前 `taskkill` 清残留；按模块表挑真游戏（`frida_probe.py`） |
| `couldn't invoke method XXX needs 0 params not 1` | 实例方法多传了 `this` | bridge 约定 `obj.method(名,参数个数).invoke(实参...)`，`this` 由 obj 绑定 |
| 矩阵/坐标/血量是 null 或乱值 | 32 位按值返回结构体 ABI 不稳 / `ObscuredInt` 是密文 | 走 `*_Injected(out)` + 裸内存读 `hiddenValue ^ currentCryptoKey` |
| 注入后游戏瞬间消失、无异常栈 | 调了有方法体的托管方法（跨线程）或泛型共享方法 | 取帧调度到 Unity 主线程；`isDead` 由血量推导；单例走 backing field / gc.choose |
| `allPlayers` 恒空不报错 | `Il2Cpp.Array` 无 `arr[i]` 下标代理 | 用 `.get(i)`，长度加 `MAX_PLAYERS` 上限 |
| 屏幕坐标离谱（如 4121） | 数学对，本地玩家压相机头 → w 极小 | 加 NDC 裁剪（`|ndc|>3` 跳过），`world_to_screen` 保留原值 |
| 框整体偏移 / 比例不对 | DPI 未感知（逻辑像素被系统放大）/ 帧坐标≠屏幕坐标 | 声明 DPI 感知；`compute_viewport` 用 letterbox |
| 画到别的窗口 / 闪烁 | 游戏失焦未隐藏 / 用 `lift()` 抢焦点 / 没排除自身前台 | `clip` 按 pid 找窗口 + 0.35s 迟滞 + 原生 `SetWindowPos` 重申顶层 |
| 游戏里点不动 | 叠加层拦了鼠标 | 补 `WS_EX_TRANSPARENT` + `WS_EX_NOACTIVATE` |
| 偶发崩溃/撕裂 | 在非 Tk 主线程碰 canvas | 跨线程只入队，主线程 `_tick()` 统一重绘 |

### C++ D3D11 叠加层（`cpp_overlay/`，详见 `CPP_OVERLAY.md`）
| 现象 | 根因 | 修法 |
| --- | --- | --- |
| 日志全绿但屏幕看不到任何内容 | `WS_EX_LAYERED` 未设分层属性 → 窗口整体不可见；且阻断 flip（0x887A0001） | 窗口创建**去掉** `WS_EX_LAYERED`；flip+PREMULTIPLIED 透明，BLT 回退才补 `WS_EX_LAYERED`+`LWA_COLORKEY` |
| `flip_hr=0x887A0001` | `WS_EX_LAYERED` 与 flip 交换链互斥（DXGI 硬规则） | 去掉 `WS_EX_LAYERED` 后 flip 即可成功 |
| `scale=inf` | 无数据时 frame 尺寸非法，视口缩放分母=0 | 分母兜底 1280x720 + 校验 `width>0 && height>0` |
| 透明窗口是黑块 / 不透明 | BLT+UNSPECIFIED 缓冲不透明，没走 colorkey | BLT 路径：清屏不透明黑 + `LWA_COLORKEY` 抠纯黑 |
| 看不到内容（怀疑渲染没输出） | 先加强制红框 + 黑底白字 HUD 验证管线 | 见 `CPP_OVERLAY.md` §4③ |
| 菜单按钮点不中，点到背后窗口 | `WS_EX_TRANSPARENT` 常开 → 整窗穿透 | 每帧轮询 `GetCursorPos`，悬停菜单矩形时动态移除该样式；**不要**用 `HTTRANSPARENT`（跨进程不可靠） |
| 菜单中文全显示 `????` | ImGui 默认字体无 CJK 字形 | 运行时加载系统 CJK 字体（`msyh.ttc` 等候选），失败退回 ASCII；屏上框标签为 ASCII 以保契约 |
| INSERT 切菜单与注入键冲突 | 键位占用 | 菜单=HOME、ESP 绘制层=DELETE，`ucf_overlay.ini` 可配；旧 ini 的 INSERT 启动时自动迁移 |

### 数据契约 / 投影（`STATIC_REFERENCE.md`）
| 现象 | 根因 | 修法 |
| --- | --- | --- |
| `team` 全员被当敌人 | `get_team()` 返回 boxed 对象，bridge 不解包 `value__` | 裸读 `Entity.<team>k__BackingField`（int）比较 |
| `ObscuredInt` 解密错 | 字段偏移 / 密钥读错 | 裸内存 `hiddenValue ^ currentCryptoKey`，用 `fakeValue` 交叉验证 |
| 投影公式对不上 | 列主序索引 / `VP=P*V` 顺序错 | 统一 `m[col*4+row]`，与 `src/projection.cpp` 一致；改前跑 `test_dump_contract.py` |

---

## 三、通用铁律（改代码前默念）

1. **边界**：只动自己的单机游戏；任何在线多人 / 第三方进程的操作都不做。
2. **数据契约联动**：改 `Frame`/`PlayerState` 必须同步 `shared_state.hpp` + `esp_core.py` + `frida_dump.js` + `STATIC_REFERENCE.md`，并跑契约测试 + cpp_overlay ctest。
3. **DXGI**：flip 交换链**禁止** `WS_EX_LAYERED`；透明优先 flip+PREMULTIPLIED，BLT 只占兜底。
4. **IL2CPP 调用**：先看 dump 里有没有 `[CompilerGeneratedAttribute]`；没有 = 有方法体 = 必须在 Unity 主线程调用，否则走「裸内存读 / 静态字段 / 堆扫描」避开 invoke。
5. **推送**：SSH key push；**不**把 PAT 写进 remote URL / `.git/config`；构建失败日志由用户手动贴回。
6. **踩坑即记**：任何新坑按本文件模板追加，并在对应 deep-doc 补「排错」细节。

### 2026-09-18：菜单、ESP 与纯角度目标输出统一接线

- **现象**：菜单只有基础框/血条开关，距离与颜色不能完整控制；绘制层没有骨骼入口，目标选择只有最近/低血量。
- **根因**：`Settings`、`DrawStyle`、绘制原语和主循环之间缺少完整的状态传递；`Frame` 没有新增可见性/骨骼字段。
- **修法**：扩展现有 `key=value` 配置与菜单；`project_frame()` 使用已有 `inGame/isDead/team/pos` 做有效性、死亡、队伍和距离过滤；当时骨骼显示采用投影框内的调试骨架，后续已由真实 `BoneState[19]` 契约替换；`smooth` 新增准星最近与方向角计算，主循环只显示平滑后的 yaw/pitch。
- **验证**：核心 ctest、dump 契约、叠加层几何、投影校准和脚本配置测试全部通过；Windows D3D11/ImGui 编译仍需 Actions 或 Windows MSVC 实机验证。
- **详见**：`cpp_overlay/src/menu_win32.cpp`、`overlay_viz.cpp`、`smooth.cpp`、`main_win32.cpp`。

### 2026-09-18：真实 Humanoid 骨骼契约与读取链

- **证据**：`dump.cs` 中 `Player.characterContainer@0x4C`、`Entity.characterAnimator@0x24`、`Animator.GetBoneTransform(HumanBodyBones)` 和 `Transform.get_position_Injected(out Vector3)` 均存在。
- **修法**：`PlayerState` 增加固定 19 槽 `BoneState`；Frida 在 Unity 主线程读取每根骨骼，缺失节点输出 `valid=false`；Python 投影和 C++ 叠加层使用同一槽位/连接表。
- **显示**：菜单 ESP 子菜单的“显示方框”和“显示骨骼”独立控制；骨骼线只在两个端点均有效且在 NDC/视口内时绘制。
- **验证**：契约测试新增 Animator/字段/方法检查；共享内存打包长度 `23804`、FrameSlots 长度 `23808` 校验通过；bundle `node --check` 通过。
- **详见**：`tools/frida_dump.js`、`tools/esp_core.py`、`cpp_overlay/tools/shm_writer.py`、`cpp_overlay/src/shared_state.hpp`、`overlay_viz.cpp`。

### 2026-09-18：退出路径默认保留配置/日志

- **修法**：`Settings.exit_delete_config` 与 `Settings.exit_delete_log` 默认均为 `false`；菜单“退出设置”只显示这两个删除选项和“退出程序 (END)”按钮。
- **触发**：菜单按钮与 `VK_END` 都设置统一的退出请求；退出前保存配置并记录步骤。
- **资源**：`SharedTransport` 使用持有对象，退出时显式析构，始终执行 `UnmapViewOfFile + CloseHandle`；Windows 不执行共享内存删除。
- **验证**：核心配置 round-trip 检查默认不删除；Windows 完整路径需 Actions/MSVC 验证文件保留/删除结果。

### 2026-09-18：DEV JSONL 录制与 DEBUG 菜单

- **决策**：录制点放在 Python `on_message()` 的原始 Frame 分支、C++ 共享内存读取之后，均早于投影，避免把投影结果误当作数据源。
- **格式**：逐行 JSONL，`schema_version=2`；回放按行解析，损坏行跳过并报告行号。
- **限制**：C++ 菜单负责实时录制和调试显示，完整 JSONL 回放/统计由 `tools/replay.py` 离线完成，避免在 Win32 渲染层引入 JSON 依赖。
- **验证**：`test_record_replay.py` 覆盖采样间隔、最大帧数、骨骼统计与 JSONL 读取；Windows D3D11 菜单需 Actions/MSVC 验证。

### 2026-09-18：录制失败退避与 Win32 菜单键盘输入

- **录制失败**：`fopen` 失败时设置 `g_record_open_failed` 并关闭录制开关；只有用户再次点击开始录制或修改路径后才清除，避免每帧重试刷日志。
- **路径**：启动时将相对录制路径解析到 exe 所在目录，并自动创建父目录；失败日志包含 `errno` 和最终路径。
- **输入焦点**：点击穿透不仅要移除 `WS_EX_TRANSPARENT`，还要临时移除 `WS_EX_NOACTIVATE` 并设置窗口焦点，否则 ImGui `InputText` 光标可见但收不到键盘字符/复制快捷键。
- **日志选择**：日志尾部改为低频刷新到只读 `InputTextMultiline`，支持鼠标选择与 Ctrl+C，同时避免每帧打开日志文件。
- **骨骼名称**：调试名称数组必须严格按 `HumanBodyBones` 数值槽位排列，不能按人体展示顺序重排。

### 2026-09-18：ESP 最大距离与通用渲染增强

- **最大距离**：旧条件 `max_distance > 0 && dist > max_distance` 把 0 当成无限距离，导致菜单调到 0m 仍显示。现改为 `max_distance <= 0` 直接过滤，0m 表示零显示范围；1m 按 `ScreenMark.dist` 的世界单位过滤。
- **统一过滤**：ESP 绘制层现在同时应用本地/队友/敌人开关，避免菜单开关只改状态但不影响 DrawList。
- **通用视觉**：血条按血量绿-黄-红渐变，盒子增加半透明填充与黑色描边，文字增加阴影；新增可选 FOV 圆。未引入第三方项目的游戏专属数据或输入注入。

### 2026-09-18：Aimbot 选中态、FOV 圈与射线

- **FOV 圈**：显示条件只依赖 `show_fov_circle`，不再要求同时开启角度计算；半径按视口短边和 FOV 比例计算，避免 180° 时 `tan(90°)` 越界。
- **目标索引**：`select_target()` 返回 `f.players[]` 索引，绘制契约中的 `ScreenMark` 还包含 local，因此高亮索引必须使用 `target + 1`。
- **视觉反馈**：选中目标使用实线框/实线骨骼；其他实体使用低亮度四角框/虚线骨骼；射线只从屏幕底部或中心指向选中实体。
- **边界**：射线和高亮仅用于显示当前纯角度选择结果，不写鼠标、不写输入。

### 2026-09-18：选靶规则改为屏幕中心最近

- **规则**：Aimbot 目标选择使用投影后的 `ScreenMark` 坐标，在 FOV 圆内按屏幕中心距离平方取最小值；不再用固定世界方向或仅按世界距离/血量决定目标。
- **一致性**：FOV 圆半径与选靶半径共用 `fov/90 * min(width,height)/2`，保证视觉圆和实际筛选区域一致。
- **角度输出**：选中索引仍使用相对世界位置计算目标 yaw/pitch，但选择依据是屏幕空间；不写游戏内存/不 Hook。本地鼠标模拟（SendInput/mouse_event）在根 AGENTS.md §0.1 授权下允许，须为独立 `input_sim/` 模块、默认 OFF。

### 2026-09-18：双选靶方案与取点配置

- **方案 A**：角度空间 FOV，使用 `w2c` 推导相机前向量，按目标取点方向与前向量的角差选择。
- **方案 B**：屏幕空间 FOV，使用目标取点的投影坐标，按屏幕中心距离选择；与可视化 FOV 圆共用半径。
- **取点**：身体中心、头部 `bones[10]`、胸部 `bones[8]`、指定骨骼槽位；无效骨骼不作为有效候选。
- **边界**：取点只影响本地调试目标选择和角度输出，不写视角内存、不 Hook、不注入到游戏进程。本地鼠标事件模拟见根 AGENTS.md §0.1（独立模块、默认 OFF）。

### 2026-09-18：Frida 真实帧接入 UcfFrame

- **现象**：C++ 叠加层能读合成帧，但 `frida_host.py` 的真实帧没有写入共享内存。
- **根因**：原宿主只把帧投影后推给 tkinter，`SharedTransport("UcfFrame")` 没有真实写端。
- **修法**：新增 `--shm`，复用 `cpp_overlay/tools/shm_writer.py` 的布局编码器，按后台槽写入、翻转 `cur` 的双缓冲顺序写入 `UcfFrame`；共享内存写失败只报一次并停桥，避免回调刷屏。
- **验证**：离线校验 `FRAME_FMT`/`SLOT_FMT` 与 `build_frame()` 长度；Windows 实机需分别启动 `frida_host.py --shm` 与 C++ exe 验证源切换。
- **详见**：`tools/frida_host.py`、`tests/test_shm_bridge.py`、`docs/HOST_OVERLAY.md`。

### 2026-09-18：真实帧落盘与投影一致性

- **落盘**：`--dump-frame DIR` 保存原始 Frame，不保存投影后的 marks；默认每帧一份、最多 500 份，可用 `--dump-every` 与 `--dump-max-frames` 调整，写临时文件后原子替换，避免留下半个 JSON。
- **双缓冲**：`frame_bytes=23804` 是一个 Frame，`slot_bytes=23808` 多出的 4 字节是当前槽索引；写端必须先写后台槽，最后翻转索引。名称 `UcfFrame` 区分大小写，退出必须关闭映射句柄。
- **失败策略**：共享内存写失败只记录一次并停止桥接，避免每帧刷屏，也避免继续运行一个已经不可信的半死写端。
- **一致性**：`tests/test_projection_parity.py` 从 `real_frame_level3.json` 生成同一组矩阵、世界点和 Python 期望值，交给 `test_projection_parity` 使用 C++ `world_to_screen` 校验，误差阈值为 `1e-4`。

### 2026-09-18：目标状态与死亡切换防抖

- **状态来源**：`TargetState` 只使用当前 `Candidate` 的 `valid/dead/hp/visible`；现有 `Frame` 没有真实遮挡字段，因此 `visible=false` 不能解释为已完成游戏内 LOS 检测。
- **防抖**：`aim_lock_prevent` 开启且本地激活状态保持时，上一个目标变为死亡、阻挡或无效并切换到新目标时只跳过一次重锁定；状态松开后复位。
- **显示**：异常状态的选中射线使用黄色；该逻辑只生成 DrawList，不调用鼠标、键盘、视角写入或任何输入 API。
- **配置**：`aim_lock_prevent` 已加入 key=value 保存/加载和 Win32 菜单，并由 `overlay_tests` 覆盖状态、一次性跳过与 round-trip。

### 2026-09-18：本地输入模拟模块隔离

- **结构**：输入模拟只放在 `cpp_overlay/src/input_sim/`，`smooth.cpp` 和 `select_screen_target()` 不依赖 Win32 输入 API。
- **默认**：`input_sim_enabled=false`；关闭时主循环不产生本地输入事件，现有只读角度输出行为保持不变。
- **语义**：按键上升沿执行一次 tap move/对齐开火，后续按住帧只执行 trace move；异常目标状态直接作为硬门，不产生 move 或开火事件。
- **测试**：Linux 核心使用 `Sender` mock 覆盖默认关闭、tap/hold、容差开火和异常状态门；Windows 后端使用 `SendInput`，需由 Actions/MSVC 验证编译。

### 2026-09-18：真实帧共享内存写入的坐标格式兼容

- **现象**：`frida_host.py --shm` 能打开 `UcfFrame`，但第一帧写入失败并停止桥接。
- **根因**：Frida Frame 的玩家 `pos` 可能是 `{x,y,z}` 字典，而 `shm_writer._ps_items()` 原先只按 `[0],[1],[2]` 列表读取。
- **修法**：`shm_writer.py` 新增 `_vec3()`，同时兼容字典和列表/元组；宿主异常日志改为 `repr + traceback`，保留完整类型、消息和行号。
- **验证**：`test_shm_bridge.py` 新增字典坐标打包用例，Frame 长度为 `23804`，单槽描述为 `23808`，双缓冲共享区为 `47612`。

### 2026-09-18：双缓冲共享区大小必须包含两个 Frame

- **现象**：第一帧可能写入成功，切换到 `back=1` 后出现 `mmap slice assignment is wrong size`。
- **根因**：`SLOT_FMT.size` 只是 `cur + 一个 Frame` 的描述，不能作为整个双缓冲共享区大小；旧 `SHM_SIZE` 少分配了一个 Frame。
- **修法**：`SHM_SIZE = 4 + 2 * FRAME_FMT.size = 47612`；C++ `FrameSlots` 增加 ABI 静态断言；写入前检查 `start + frame_size <= len(shm)`。
- **验证**：共享区长度、两槽边界、字典坐标打包和 C++ ctest 均通过。

### 2026-09-18：第三人称本地点的 0.95m 与重复 local 记录

- **现象**：真实帧中 `local` 与 `players[0]` 位置完全相同，日志出现两条相同的爆炸屏幕坐标，并显示距离约 `0.95m`。
- **判定**：`camera_position(w2c)` 按 `-Rᵀ·t` 计算后，相机确实位于本地玩家约 `0.95m` 处；这是第三人称近相机投影的正常结果，不是把 `w2c.col3` 直接当相机位置。
- **修法**：Python 投影跳过 `isLocal`/`isMyPlayer` 或与 `Frame.local` 重合的记录；共享内存写入端在 `Frame.local` 已单独保存后过滤同一重复项，避免 C++ 再画一次。
- **验证**：投影校准确认本地点仍被近裁剪；重复项只投影一次；真实矩阵的 `P*V`、相机位置和屏幕坐标回归均通过。

### 2026-09-18：C++ 视口必须使用游戏客户区而不是整屏

- **现象**：游戏 Frame 为 800x600，桌面/叠加层为 1536x864 时，旧代码使用 `sx=1.92`、`sy=1.44`，窗口化或 4:3 全屏会出现框偏移和非等比拉伸。
- **修法**：按 `UnityCrossFire.exe` 进程查找最大可见窗口，取客户区桌面坐标；`auto` 在宽高比不一致时使用等比居中，`stretch` 和 `letterbox` 可通过命令行覆盖；FOV 也按同一 Viewport 映射。
- **验证**：核心 `compute_viewport` 覆盖 1536x864/800x600 的 letterbox、stretch 及窗口化偏移场景；日志记录 viewport 和双轴缩放。
