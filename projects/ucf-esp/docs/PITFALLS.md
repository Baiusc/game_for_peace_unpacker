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
