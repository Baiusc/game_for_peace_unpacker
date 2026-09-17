# 项目长期记忆（ucf-esp / game_for_peace_unpacker）

## 用户真实游戏（已授权，单机）
- 游戏名：**UnityCrossFire**（U3DCF 单机版 30 人生化 UCF 新年版本），厂商署名 `Alexander_GaGa`。
- 可执行文件：`UnityCrossFire.exe`；IL2CPP 32 位（`UnityCrashHandler32.exe` + `UnityPlayer.dll` + `GameAssembly.dll`）。
- 安装/运行目录示例：`C:\aG\U3DCF单机版30人生化UCF新年版本\`（目录含中文，frida 最好传完整路径或在游戏目录内运行）。
- `projects/ucf-esp/il2cpp_dump/dump.cs` 即来自此二进制；类布局 `GameManager`/`Player`/`Camera`/`ObscuredInt` 已按本游戏核对，无需改偏移。
- 配置存档：`%LOCALAPPDATA%\..\LocalLow\Alexander_GaGa\UnityCrossFire\PlayerData.dat`（玩家名/军衔/VIP，与 ESP 无关）。
- 框架内置 Anti-Cheat Toolkit（`WallHackDetector`/`InjectionDetector`），调试期须用关闭反作弊的构建，否则 Frida 注入误报。

## ucf-esp 工程定位
- 给自己单机游戏做的**调试/可视化叠加层**（开发期工具），非联机作弊。脚本默认进程名已从误写的 `u3dcf.exe` 改为 `UnityCrossFire.exe`。
- 矩阵约定：列主序扁平 16（`m[col*4+row]`），与 `src/projection.cpp` 一致；`VP = combine_pv(proj, w2c)`。
- 数据流：`frida_dump.js`（frida-il2cpp-bridge 读矩阵+玩家）→ send(frame) → `frida_host.py`（project_frame）→ `overlay_tk.py`（tkinter 透明叠加层）/ `replay.py`（离线验证）。
- 沙箱限制：bash 环境损坏无法构建 C++ 投影实验室；Python 可用，已用 `validate_projection.py` / `replay.py` 验证黄金样本。

## 运行环境与关键坑（2026-09-17 实测）
- 本机 Python：`C:\Users\Administrator\AppData\Local\Programs\Python\Python312\python.exe`（64 位，装有 **frida 17.18.0**）。64 位 frida 注入 32 位 IL2CPP 进程**正常**，不需要装 32 位 Python。
- **挑进程必须看模块表**：同名 `UnityCrossFire.exe` 常有多个（真游戏几百 MB + 若干 ~10MB 空壳，重复启动残留）。按“第一个同名匹配”attach 会注进空壳 → `IL2CPP module 'GameAssembly.dll' has not been loaded yet`。用 `tools/frida_probe.py` 或 `frida_host.py --list` 找“有 `GameAssembly.dll`”的 pid。
- 宿主默认流程：`taskkill /F /T /IM` 清残留 → 启动唯一实例 → 等 `INS` → attach（见 `docs/HOST_OVERLAY.md` 参数表）。
- 改 `frida_dump.js` 后**必须重打包 bundle**：在 `C:\aB\ucf_bundle_tmp`（已装 `frida-il2cpp-bridge` 0.14.0 + `esbuild` 0.21.5）跑 `esbuild --bundle --format=iife --platform=neutral`，**输出本地文件名再 `cp` 回 `tools/`**（Git Bash 传 `--outfile=/c/...` 会被 esbuild 当成 `C:\c\...`）。
- Windows 小坑：`taskkill` 输出是 GBK（`subprocess text=True` 会 `UnicodeDecodeError`，按字节读再 `decode("gbk", errors="replace")`）；全局热键用 `GetAsyncKeyState` 轮询，不要用 ctypes 低级键盘钩子（64 位下 `lParam` 溢出必崩）。

## frida-il2cpp-bridge 调用约定（踩坑后确认，改脚本前必读）
1. **实例方法不传 this**：`obj.method(名, 参数个数).invoke(实参...)`。`invoke(cam)` 这种多传一个会报
   `couldn't invoke method X as it needs 0 parameter(s), not 1`。静态用 `Klass.method(...).invoke(...)`。
2. **32 位不要依赖“结构体按值返回”**：`Matrix4x4`(64B)/`Vector3`(12B) 走 `*_Injected(out T)` + `Memory.alloc` 缓冲，
   再裸内存读 float。`frida_dump.js` 会探测并打印 `矩阵读取模式: injected|getter`。
3. **值类型（struct）字段别用 bridge 的字段访问链**：`ValueType` 的字段/方法绑定带 `−headerSize` 补偿，
   从“字段读出的 ValueType”再取字段会差一个 header。血量因此改裸内存 `hiddenValue ^ currentCryptoKey`。
4. `Class.tryMethod` 走 `classGetMethodFromName`：能搜到 private 方法（如 `_Injected`），也沿父类链
   （`GameManager.get_instance` 实际声明在 `Singleton<T>` 上）。
5. 原始类型返回：数字原样、bool → `!!value`、enum → number。
6. 统一入口 `callMethod()` 在 `tools/frida_dump.js`：`obj.tryMethod` 缺失时沿 `obj.class.hierarchy()` 找声明类再 `bind(obj)`。
7. **绝不要调泛型基类上的静态泛型方法**（`Singleton<T>.get_instance()`）：dump 里它是 `RVA:-1`
   的共享泛型方法，且 IL2CPP 共享泛型带**隐藏 `MethodInfo*` 参数**，按普通签名调用 → 被调方
   解引用垃圾指针 → **AV、进程瞬间消失、Player.log 无任何异常栈**（level3/4 崩溃真凶）。
   取单例改走：静态 backing field `Singleton<T>.<instance>k__BackingField`（零 invoke）
   → 失败再 `Il2Cpp.gc.choose(GameManager)` 堆扫描（5s 缓存）→ `get_instance()` 仅作兜底且默认关闭。
8. **`Il2Cpp.Array` 没有下标 Proxy**：只有 `.length` / `.get(i)` / 迭代器。写 `arr[i]` 恒为
   `undefined`，会导致 `players` 永远为空且不报错。数组长度要加上限（现 `MAX_PLAYERS=128`）防读到垃圾值。
   （实测 `allPlayers.length=30` 但只有 10 个非 null：数组是预分配槽位，null 要跳过。）
9. **enum 返回值是 boxed 对象，不解包 `value__`**（bridge 只在 type 为 VALUE_TYPE+isEnum 时解包，
   `get_team()` 走的是 OBJECT 分支）→ 传到宿主是 `{handle,type}`，**两个对象永不相等 → 队友识别
   整个失效、全员被判 enemy**。改裸读 backing field 的 int：`Entity.<team>k__BackingField`@0x1C
   （`Team` 是 `enum:int`，0=BlackList 1=GlobalRisk 2=Neutral），见 `readTeamOf()`。
10. **【最重要】frida 线程 ≠ Unity 主线程**：`setInterval` 跑在 frida 自己的线程上。
   调"有真实方法体"的托管方法会 **AV、进程瞬间消失**。**判读口诀：看 dump 里有没有
   `[CompilerGeneratedAttribute]`** —— 有 = 只读取字段，安全；**没有 = 有真实方法体，危险**。
   实测：`Entity.get_team()`(0x1D80E0，有)✓ / `Entity.get_isDead()`(0x1D80B0，**无**)✗崩 /
   `Player.get_isMyPlayer()`(0x2C3C80，**无**)⚠ / `HealthData.isDead`(0x213B70，**无**)⚠。
   对策：① 取帧默认调度到 Unity 主线程 —— `Il2Cpp.mainThread`（=attachedThreads[0]）
   + `Thread.schedule(block)`（走 Unity SynchronizationContext.Post，**返回 Promise**，
   需 `MT.busy` 防异步重入堆积），`--no-main-thread` 可关；② 能推导的别调方法
   —— `isDead` 改由 `hp<=0` 推导（血量本就是裸内存明文，零 invoke）。

## ucf-esp 离线回归（每次改完必跑，都不需要游戏）
- `python tests/test_process_selection.py`  进程选择 + `--game` 路径解析（8 例）
- `python tests/test_dump_contract.py`  解析 `il2cpp_dump/dump.cs` 校验偏移/方法名/列主序/Team 枚举（9 项）
- `python tests/test_projection_calibration.py`  真实对局帧锁死投影数学 + NDC 裁剪（24 项，含合成样本不被误裁）
- `python tests/test_script_config.py`  档位/单步探测/UCFG 透传/payload 解析/isDead 推导/主线程调度，含"源码与 bundle 是否同步"（10 项）
- `python tools/replay.py --golden`  黄金样本 3/3（`(-0.35,0.55,0)→(416,162)` 等）
- `--game` 支持裸文件名：按 环境变量 `UCF_GAME` → 已知安装目录 → 当前目录 解析（`resolve_game_path()`）。

## 注入后闪退的定位手段（`--level` 二分 + 宿主看门狗）
- 宿主默认先清残留 → 启动 → 按 INS 注入；注入后若进程消失，看门狗会打印
  `游戏进程 <pid> 已退出（注入后仅存活 X.Xs）` 并自动打出 Unity 日志尾部：
  `%LOCALAPPDATA%\..\LocalLow\Alexander_GaGa\UnityCrossFire\Player.log`。
- **判读**：Player.log 有异常栈 = 我们自己的调用崩了；日志干净却静默消失 = 外部终止（ACTK/被杀）。
- **二分**：`--level 0`（不碰 IL2CPP，只心跳）→ 若仍闪退 = 注入本身被检测，**应关 ACTK 构建而非对抗**；
  `--level 1` 只初始化 IL2CPP；`--level 2` 只读矩阵；`--level 3` +玩家不含血量；`--level 4` 全量（默认）。
  配套 `--interval 1000` 降频、`--discover` 打真实方法名。
- 档位靠宿主在脚本前拼 `const UCFG={level,interval,discover};`，frida_dump.js 读 `UCFG`；
  直接用 `frida -l bundle` 时无 UCFG → 回退 level=4。
- 进程探活用 `kernel32.OpenProcess(0x1000)`，不要用 tasklist（慢 + GBK 解码坑）。
- **已定性（2026-09-17 实测）**：`--level 0` 心跳 14 次存活 → **ACTK 没检测 frida 注入**；
  `--level 2` 矩阵读出且不崩 → **矩阵 `_Injected` 链路正常**。剩余嫌疑在 level3（遍历玩家/坐标）
  与 level4（ObscuredInt 裸读）。之前的"闪退"是 level3/4 层的问题，不是反作弊。
- **frida payload 坑**：frida 16+ 把 `send(对象)` **直接解成 dict**，`json.loads(payload)` 会报
  `the JSON object must be str... not dict` 并在 20Hz 下刷屏。宿主已用 `_coerce_payload()`
  兼容 dict/str/bytes，且同类错误只报一次；控制台打印节流 2Hz。
- **层内定位用 `--probe` 单步探测**（`--level` 只能缩到一层）：注入后按固定顺序把 16 个关键调用
  各做一遍，每步前后打日志、步间 250ms（用 setInterval 状态机，不用 async/await）；
  **进程崩掉时最后那行 `▶ [n/16]` 即崩点**。高危的 `get_instance()` 刻意排最后。
  探测跑完才 `startFrameLoop()`。
- **判读补充**：`application-requested` = 用户 Ctrl+C；`process-terminated` = 游戏进程真死了。

## 投影：公式是对的，别再怀疑（2026-09-17 手算验证）
- 本地玩家被投影成 `screen=(-153,4121)`（屏幕 800x600）**不是公式错**。用行主序 `P·V`
  独立复算 = `(-152.6, 4121.1)`，逐位吻合；tick=3 帧敌方复现实测 `(797,215)`（实算 797.4/215.3）。
- **真因**：第三人称相机挂在玩家头顶 0.95m，`cam = -Rᵀ·t`（`esp_core.camera_position()`，
  列主序取 `R[row][col]=V[c*4+r]`、`t=V[3*4+r]`）≈ (10.688,14.170,-2.066)，玩家 (10.57,13.23,-2.00)。
  透视除法 w=0.1096 → ndc_y=-12.73 → 坐标放大几十倍。数学正确但不可画。
- **裁剪判据必须用 NDC 幅度（`NDC_CLIP=3.0`），不能用距离**：先写 `dist<1.5m`，
  结果合成样本（尺度是"1 单位"不是米，玩家离相机 0.65）被误裁到全不可见。
  `world_to_screen` **不做裁剪**、仍返回原始值（便于诊断），裁剪只在 `_make` 标 `clipped`。
- Mark 新增 `dist`（到相机距离）与 `clipped`；打印成 `[不可画: ndc 超界, 0.95m]`。
- **叠加层对齐（方向别搞反）**：投影用 frame 的 width/height，所以**画布尺寸必须等于帧分辨率**；
  游戏窗口客户区只提供**位置**。旧代码硬编码 `OverlayWindow(1280,720)` 而游戏是 800x600
  → 框放大 1.6 倍错位。`overlay_tk.game_client_rect()` 用 `FindWindowW("UnityWndClass")`
  + `GetClientRect` + `ClientToScreen`（**刻意不用 EnumWindows 回调**：64 位 ctypes 回调
  会 OverflowError）。**必须校验客户区尺寸 == 帧分辨率（±2px）才采信其位置** —— 别用宽高比
  判断，实测抓到过无关的 320x240 Unity 窗口（和 800x600 同为 1.333）。锚点确认后锁定。
  `Screen.get_width/height()` 是渲染分辨率，开了 render scale 时会≠客户区，画布仍按它走。
- **已结案（2026-09-17）**：`--probe --level 3` 把崩点钉死在 `▶ [13/16] player.get_isDead()`
  （前 12 步全通：`allPlayers.length=30`、坐标 `(7.53,11.97,25.24)`、`team=GlobalRisk`）。
  → 真因是 frida 线程调有方法体的 getter；已用"主线程调度 + isDead 由 hp 推导"修掉。
  `get_isDead()`/`get_instance()` 均降级为可选开关（`--allow-is-dead` / `--allow-get-instance`），
  在探测里分别排倒数第二和最后。
