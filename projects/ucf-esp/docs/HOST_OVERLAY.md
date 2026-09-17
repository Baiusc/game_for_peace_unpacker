# UCF-ESP 宿主端 + 叠加层

把 `frida_dump.js` 读出的相机矩阵 / 玩家数据，投影到屏幕坐标并绘制调试叠加层。
仅用于你**拥有授权的单机游戏 UnityCrossFire**；请勿用于任何联机或他人游戏。

## 文件

| 文件 | 作用 |
|---|---|
| `tools/esp_core.py` | 投影数学（`combine_pv` / `world_to_screen`）+ 帧处理（`project_frame` → `Mark` 列表）。复刻 `src/projection.cpp` 列主序约定，与 `validate_projection.py` 一致。 |
| `tools/render_html.py` | 离线把 marks 渲染成静态 SVG（无游戏也可核对投影）。 |
| `tools/overlay_tk.py` | Windows 透明置顶叠加层骨架（tkinter，点击穿透）。 |
| `tools/replay.py` | 离线验证器：加载 frida 帧样本 → 投影 → 打印 → 跑黄金样本断言 → 可选 SVG。 |
| `tools/frida_host.py` | 实时宿主：**先清理同名残留进程 → 正常启动游戏 → 按 `INS` 键才把 frida 注入到真正跑 IL2CPP 的 `UnityCrossFire.exe`** → 收帧 → 投影 → 推叠加层。 |
| `tools/frida_probe.py` | 进程探针：列出所有同名进程及其 `arch` / 模块数 / 是否加载了 `GameAssembly.dll`，用来确认该注哪一个（排查“注进空壳”的利器）。 |
| `tests/test_process_selection.py` | 进程选择逻辑的离线单元测试（假进程表 + monkeypatch，不需要 frida / 游戏）。 |
| `tests/test_dump_contract.py` | 契约测试：解析 `il2cpp_dump/dump.cs`，校验 `frida_dump.js` 写死的方法名与字段偏移（矩阵列主序、`*_Injected` 签名、`ObscuredInt` 解密布局）。离线、不需要游戏。 |
| `tools/frida_dump.js` | **源码**：基于 `frida-il2cpp-bridge` 的运行时数据源（含 `DISCOVER` 开关）。 |
| `tools/frida_dump.bundle.js` | **打包产物**（自包含，已内联 bridge）。`frida_host.py` 实际加载它，无需本机 `npm i frida-il2cpp-bridge`。若改了源码需重打包（见下）。 |
| `tools/sample_session.json` | 与 `frida_dump.js` 同构的合成帧（identity VP，含三组黄金样本点），用于离线自测。 |

## 数据流

```
UnityCrossFire.exe (GameAssembly)
   │  frida-il2cpp-bridge 读 Camera/GameManager/Player
   ▼
tools/frida_dump.bundle.js  ──send(frame)──►   (由 frida_dump.js 经 esbuild 打包成 IIFE，内联 bridge)
   │                                  tools/frida_host.py
   │                                     │  esp_core.project_frame
   ▼                                     ▼
frame = { w2c[16], proj[16], local, players[] }   VP = combine_pv(proj, w2c)
                                            │  world_to_screen(pos, VP, w, h)
                                            ▼
                                      marks[ Mark{kind, screen, hp, team, on_screen} ]
                                            │
                                            ▼
                                  overlay_tk.OverlayWindow.update(marks)
```

## 离线自测（无需游戏，沙箱/本机均可）

```bash
cd projects/ucf-esp
python tools/replay.py sample_session.json --golden --html tools/sample_preview.svg
python tests/test_process_selection.py    # 进程选择逻辑（多实例/全空壳/延迟加载）+ --game 路径解析 离线断言
python tests/test_dump_contract.py        # dump.cs <-> frida_dump.js 契约（偏移/方法名/列主序布局）
```

预期：
- 打印 5 个 mark（local + 4 players），其中 `x=5.0` 的敌方被判定 `onscreen=False`。
- 黄金样本断言全部 OK：`(-0.35,0.55,0)→(416,162)`、`(0.4,-0.45,0)→(896,522)`、`(0,0,0)→(640,360)`。
- 生成 `tools/sample_preview.svg`（本地玩家绿、队友蓝、敌方红，带血条）。
- 契约测试 8 项全过（列主序布局、`*_Injected` 签名、`ObscuredInt` 偏移、JS 引用的方法名）。

## 实时运行（本机 Windows，需 frida）

```bash
pip install frida frida-tools
python tools/frida_host.py                 # 清残留 -> 启动游戏 -> 按 [INS] 注入，带叠加层
python tools/frida_host.py --no-overlay    # 仅控制台打印每帧标记
python tools/frida_host.py --list          # 只看进程：哪些同名进程？哪个加载了 GameAssembly.dll？
```

**完整流程（默认行为）：**
1. `frida_host.py` **先清掉所有同名残留进程**（`taskkill /F /T /IM UnityCrossFire.exe`），避免“一个真游戏 + 若干 ~10MB 空壳”把注入带偏；
2. 再**用普通方式启动唯一实例**（等价于双击 exe，`subprocess.Popen` + 游戏目录为 cwd）。此时进程里**完全没有 frida**，不会在 boot 阶段被反作弊误报；
3. 等你进入对局想看数据时，**按一下 `INS`（Insert）键**，它才逐个探测同名进程、挑出**真正加载了 `GameAssembly.dll` 的那个**并 `frida.attach` + 加载 `frida_dump.bundle.js`；
4. 之后每帧投影并 `OVERLAY.update(marks)`；`Ctrl+C` 退出（frida 自动 detach）。

`frida_dump.bundle.js` 以 **V8 运行时**加载（已内联 `frida-il2cpp-bridge`，本机无需额外安装 bridge），默认 **50ms 一帧**（约 20fps），并每 5 秒打一条 `已发送 N 帧` 心跳。

可选参数：

| 参数 | 作用 |
|---|---|
| `--auto` | 不等待 INS，启动即注入（旧行为） |
| `--no-launch` | 不启动游戏、**也不清理**，附加到你已经手动运行的进程 |
| `--kill-stale` | 配合 `--no-launch`：只清掉没加载 `GameAssembly.dll` 的空壳进程，**保留真游戏** |
| `--no-cleanup` | 启动前不清理同名残留进程（默认会先清干净再启动） |
| `--pid <N>` | 直接指定要注入的 pid（跳过自动挑选） |
| `--wait-inject <秒>` | 等 `GameAssembly.dll` 载入的最长秒数（默认 20） |
| `--level <0-4>` | 运行档位（默认 4 全量）。排查闪退时用：0=不碰 IL2CPP / 1=只初始化 / 2=只读矩阵 / 3=+玩家不含血量 / 4=全量 |
| `--interval <毫秒>` | 每帧采样间隔（默认 50）。排查时可改 `1000` 降频观察 |
| `--discover` | 打印 `Camera` / `GameManager` 的真实方法名与字段名，用于对齐版本 |
| `--probe` | **单步探测**：注入后把每个关键调用各做一遍并逐步打日志，进程若崩掉，最后那行 `▶ [n/N]` 就是崩点。定位闪退首选 |
| `--allow-get-instance` | 允许调用 `GameManager.get_instance()`（声明在泛型基类 `Singleton<T>` 上，共享泛型方法极易 AV，**默认不走**） |
| `--allow-is-dead` | 允许调用 `get_isDead()`（有真实方法体，就是 level3 实测崩点；默认由血量 `hp<=0` 推导） |
| `--no-main-thread` | 取帧不调度到 Unity 主线程（默认调度；frida 线程上调有方法体的托管方法会 AV） |
| `--silence <秒>` | 多少秒没收到帧就告警（默认 10） |
| `--script <文件>` | 指定要加载的 JS（默认 `frida_dump.bundle.js`） |
| `--no-overlay` | 不创建叠加窗口，仅控制台打印 |
| `--game <路径>` | 指定游戏 exe：完整路径或裸文件名都行。裸文件名会在已知安装目录 / 环境变量 `UCF_GAME` / 当前目录里找；都找不到会打印三种正确写法（不会再抛 `[WinError 2]`） |

> `INS` 用 `GetAsyncKeyState` 轮询实现（纯标准库、无第三方依赖），即使游戏窗口在前台也能触发；`ENTER` 也可；非 Windows 环境退化为“按回车注入”。
> 早期版本用的是 `SetWindowsHookEx` 低级键盘钩子，其 ctypes 回调在 64 位下会抛
> `OverflowError: int too long to convert`（`lParam` 是 64 位指针），现已移除。

> 若你修改了 `frida_dump.js` 源码后需要重打包（**必须重打包，宿主加载的是 bundle**）：
> ```bash
> # 在任意目录装依赖（国内建议加镜像源，快很多）
> mkdir -p /c/aB/ucf_bundle_tmp && cd /c/aB/ucf_bundle_tmp
> npm install frida-il2cpp-bridge esbuild --registry=https://registry.npmmirror.com
> cp <工程>/projects/ucf-esp/tools/frida_dump.js .
> ./node_modules/@esbuild/win32-x64/esbuild.exe frida_dump.js \
>     --bundle --format=iife --platform=neutral --charset=utf8 --outfile=frida_dump.bundle.js
> cp frida_dump.bundle.js <工程>/projects/ucf-esp/tools/
> ```
> 三个坑：① 用 `frida-compile` 会长时间卡住，`esbuild` 秒级完成；
> ② 在 Git Bash 里给 esbuild 传 `--outfile=/c/...` 会被当成 `C:\c\...`，
> 所以**输出到本地再拷回**（上面的写法已规避）；
> ③ 不加 `--charset=utf8` 的话中文日志会被转义成 `\uXXXX`，排查时没法 grep。
> 打包后跑 `python tests/test_script_config.py` 可校验产物与源码是否同步。
> 校验产物：`grep -c '^import ' bundle.js` 应为 `0`（IIFE 自包含），且 `node --check` 通过。

> **运行前提**
> - 这是 **32 位 IL2CPP** 游戏（`UnityCrashHandler32.exe` + `UnityPlayer.dll` + `GameAssembly.dll`）。
>   64 位 Python + frida 17.x **可以**注入 32 位进程（frida 自带双架构 agent，实测 agent 已在目标里跑起来），**不需要**装 32 位 Python。
> - 需要用完整路径时：
>   ```bash
>   python tools/frida_host.py --game "C:\aG\U3DCF单机版30人生化UCF新年版本\UnityCrossFire.exe"
>   ```
> - 你的 dump.cs 即来自此游戏，类布局（`GameManager` / `Player` / `Camera` / `ObscuredInt`）已按本目录二进制核对，无需再改偏移。
> - 若 `frida-il2cpp-bridge` 因版本不同报方法名不存在，加 `--discover`，它会打印真实的导出名与字段名供对齐。

## 排错：`IL2CPP module 'GameAssembly.dll' has not been loaded yet`

实测踩到的真凶**不是**架构问题，而是**注进了同名空壳进程**：

```
UnityCrossFire.exe  6964    10,004 K   <- 空壳（没有 GameAssembly.dll）—— 旧版按名取第一个就注这里
UnityCrossFire.exe 14500     9,732 K   <- 空壳
UnityCrossFire.exe 17304   453,196 K   <- 真游戏（有 GameAssembly.dll）
```

出现多个同名进程的原因：反复运行宿主（旧版每次都会 `Popen` 再开一个），单实例保护下留下的
存根进程会堆着不退。**现在默认启动前会先 `taskkill` 清干净**，并按模块表挑进程。

排查三步：

```bash
python tools/frida_probe.py        # 一览：每个同名 pid 的 arch / 模块数 / 有没有 GameAssembly.dll
python tools/frida_host.py --list  # 同上，并直接告诉你该用哪个 pid
python tools/frida_host.py --no-launch --kill-stale   # 清掉空壳，保留真游戏后再附加
```

判读：

| 现象 | 含义 | 处理 |
|---|---|---|
| 某个 pid `GameAssembly.dll=YES`、`modules` 上百 | 这才是真游戏 | 注它 |
| 多个 pid 都是 `no` 且 `modules` 只有几十 | 全是空壳，真游戏没起 | 关掉多余实例，重新启动游戏 |
| 目标里出现 `Mono 运行时` 标注 | 不是 IL2CPP 包 | 本方案不适用 |
| 全部 `探测失败` | 进程受保护/已退出 | 确认是不是被反作弊杀掉 |

## 排错：`couldn't invoke method XXX as it needs 0 parameter(s), not 1`

这是 `frida_dump.js` 里**给实例方法多传了一个 `this`**。bridge 的约定是：

```js
// ✅ 正确：obj.method(名, 参数个数).invoke(实参...)
cam.method("get_worldToCameraMatrix", 0).invoke()
// ❌ 错误：把实例当参数传进去
cam.method("get_worldToCameraMatrix").invoke(cam)
```

`this` 由 `obj` 绑定，`invoke()` 只接方法自己声明的参数。已修复，且新增了
`callMethod()` 统一入口（找不到时还会沿父类链找声明类，`Component.get_transform` 这类继承成员也能拿到）。

## 排错：矩阵/坐标/血量读出来是 null 或乱值

`frida_dump.js` 对这三类数据都刻意**避开了“结构体按值返回”**，走的是确定性路径：

| 数据 | 取法 | 为什么 |
|---|---|---|
| `worldToCameraMatrix` / `projectionMatrix` | `get_*_Injected(out Matrix4x4)` + 自己的 64 字节缓冲 | `Matrix4x4` 64 字节按值返回在 32 位下依赖 frida 的结构体返回 ABI，容易翻车；`_Injected` 只是 void + out 指针 |
| 玩家坐标 | `Transform.get_position_Injected(out Vector3)` + 12 字节缓冲 | 同理（Vector3 12 字节） |
| 血量 | 裸内存读 `hiddenValue ^ currentCryptoKey` | `ObscuredInt` 是内联 struct，bridge 对值类型的字段访问带 header 偏移补偿，从“字段读出来的结构体”再取字段容易错位 |

启动日志里会直接给出判读依据：

```
[*] 矩阵读取模式: injected（out 指针，32 位下最稳）
[*] ObscuredInt 自检 currentHealth: hidden=0x… key=0x… -> 100（fakeValue=100 ✓一致）
```

- 若显示 `矩阵读取模式: getter` → 说明 dump 里没有 `*_Injected`，走了回退路径，此时才可能碰到 ABI 问题。
- 血量的 `✓一致` 是交叉验证：ACTK 在 `fakeValueActive` 为真时会镜像一份明文到 `fakeValue`，两边相等即证明解密算法正确；`✗不一致` 则要回头核对 `ObscuredInt` 的字段偏移（`tests/test_dump_contract.py` 会先拦一道）。

改动 `frida_dump.js` 后这些偏移/方法名是否还对得上，跑一次契约测试即可（离线、不需要游戏）：

```bash
python tests/test_dump_contract.py
```

## 排错：注入后游戏直接消失（闪退）

症状：`frida agent loaded` → `IL2CPP ready` → `矩阵读取模式: injected` → **然后进程没了**，
宿主却还在傻等（旧版用 `sys.stdin.read()` 阻塞，看不出死在哪一步）。

现在宿主自带**看门狗**：进程一退出就立刻打印 `游戏进程 <pid> 已退出（注入后仅存活 X.Xs）`，
并把 Unity 的 `Player.log` 尾部打出来，不再需要你手动去猜。

```bash
python tools/frida_host.py --level 0 --no-overlay
python tools/frida_host.py --level 2 --no-overlay
```

### 先分清「是谁杀的」

| 线索 | 结论 |
|---|---|
| `Player.log` 里有 managed 异常栈 / `Crash!!!` | 我们自己的调用把进程搞崩了（越界、签名错） |
| `Player.log` 干净、无任何错误，进程静默消失 | 外部终止：反作弊 `Application.Quit()`，或被 taskkill |
| 注入前就活不久（不注入也闪退） | 游戏本身的问题，与本工具无关 |

`Player.log` 路径：`%LOCALAPPDATA%\..\LocalLow\Alexander_GaGa\UnityCrossFire\Player.log`
（宿主看门狗会自动打印它的尾部）。

### 用 `--level` 二分定位

逐级放开调用范围，**哪一级开始闪退，锅就在那一层**：

| `--level` | 做什么 | 闪退说明什么 |
|---|---|---|
| `0` | 连 IL2CPP 都不初始化，只每 2s 打心跳 | **frida 注入本身被检测**（如 ACTK `InjectionDetector` 扫到 agent 模块/线程）。与读数据逻辑无关，只能换关闭反作弊的构建 |
| `1` | 初始化 IL2CPP + 查类，**一个方法都不 invoke** | bridge 初始化 / 元数据扫描触发（少见） |
| `2` | 每帧只读相机矩阵（`*_Injected` out 指针） | 矩阵调用越界/签名不匹配 → 改用 `--level 2` 单步确认后回退到 `getter` 模式 |
| `3` | + `GameManager` 单例 + 遍历 `allPlayers` + 坐标/队伍，**不读血量** | 实体遍历或 `get_position_Injected` 有问题 |
| `4` | 全量：含 `ObscuredInt` 血量裸读（默认） | 血量偏移/解密越界（对照 `ObscuredInt 自检` 输出） |

配套参数：`--interval 1000` 把 20Hz 降到 1Hz（降低触发概率、也便于观察），
`--discover` 打印真实方法名/字段名，`--script <文件>` 加载别的 JS。

### 定位到具体那一次调用：`--probe` 单步探测

`--level` 只能把范围缩到一层（比如“实体遍历”），而一层里有十几次调用。
`--probe` 会把每个关键调用**各做一遍**，每步前后各打一行日志（步间 250ms，保证日志
来得及送出进程），**进程若在某一部炸掉，最后那行 `▶ [n/N]` 就是崩点**：

```powershell
python tools\frida_host.py --probe --level 3 --no-overlay --interval 1000
```

```
[*] 单步探测开始：共 16 步，每步间隔 250ms（崩了看最后一行 ▶）
▶ [1/16] Camera.get_main() ...
   ✓ Camera.get_main() → 0x1a2b3c40
▶ [2/16] 矩阵 *_Injected(out) ...
   ✓ 矩阵 *_Injected(out) → w2c[0..3]=1.00,0.00,0.00,0.00
...
▶ [16/16] 【最后·高危】GameManager.get_instance() ...
   ✗ 【最后·高危】GameManager.get_instance() 抛异常: ...
[*] 单步探测全部完成，开始正常取帧
```

步骤顺序（详见 `frida_dump.js` 的 `buildProbeSteps`）：
`Camera.get_main` → 矩阵 `*_Injected` → `Screen` 尺寸 → 静态字段 `myPlayer` →
静态字段 `<instance>k__BackingField` → 堆扫描 `gc.choose` → `gm.allPlayers` →
`length` → `get(0)` → `get_transform` → `get_position_Injected` → `get_team` →
`get_isDead` → `get_isMyPlayer` → `get_healthData` + `ObscuredInt`（仅 `--level 4`）→
**最后**才是 `get_instance()`。

> `get_instance()` 刻意排在最后：它是最危险的一步（见下），
> 万一它崩了，前面 15 步的结论也已经拿到了。

### 两个已知的真凶（都已修，记录在此避免重犯）

**① 泛型基类上的静态泛型方法 —— level3/4 硬崩溃的头号嫌疑**

`dump.cs` 231189：

```csharp
public class Singleton<T> : MonoBehaviour {
    // RVA: -1 Offset: -1
    public static T get_instance() { }      // ← 开放泛型定义没有本地代码
```

`GameManager : Singleton<GameManager>`，`get_instance()` 是**共享泛型方法**，
只有 `Singleton<GameManager>` 的实例化版本才有真实 RVA（`0x4953F0`）；
而且 IL2CPP 的共享泛型方法还带一个**隐藏的 `MethodInfo*` 参数**，
按普通签名调用会让被调方拿到垃圾 `MethodInfo*` 并解引用 → **直接 access violation，
进程瞬间消失、`Player.log` 里连异常栈都没有**（正是之前看到的现象）。

现在取单例改走两条**不 invoke 任何方法**的路：

1. 读编译生成的静态 backing field `Singleton<T>.<instance>k__BackingField`；
2. 不行再 `Il2Cpp.gc.choose(GameManager)` 堆扫描（带 5s 缓存）。

`get_instance()` 只是最后兜底，**默认关闭**，需要时加 `--allow-get-instance`。

**② `Il2Cpp.Array` 没有下标代理 —— 玩家列表恒空的静默 bug**

bridge 的 `Il2Cpp.Array` 只有 `.length` / `.get(i)` / 迭代器，**没有 `arr[i]` 的 Proxy**。
旧代码写 `allPlayers[i]` → 永远是 `undefined` → `if (p)` 判空直接跳过 →
`players` 一直是空数组，还不报错。现在改用 `.get(i)`，并对 `length` 加了
`MAX_PLAYERS=128` 上限（读到垃圾长度时截断，而不是一路越界）。

**③ 在 frida 线程上调「有真实方法体」的托管方法 —— `get_isDead()` 崩的真因（`--probe` 实测）**

`--probe` 把崩点钉死在 `▶ [13/16] player.get_isDead()`，前 12 步全过。对照 dump.cs，
规律极其清晰 —— **看有没有 `[CompilerGeneratedAttribute]` 就能预判安不安全**：

| 方法 | dump 里的标记 | 结果 |
|---|---|---|
| `Entity.get_team()` @`0x1D80E0` | ✅ `[CompilerGenerated]`，只是读字段 | 通过 |
| `Entity.get_isDead()` @`0x1D80B0` | ❌ **无**（有真实方法体） | **AV，进程终止** |
| `Player.get_isMyPlayer()` @`0x2C3C80` | ❌ **无**（有真实方法体） | 同类隐患 |

根因是**线程归属**：frida 的 `setInterval` 跑在 **frida 自己的线程**上，不在 Unity 主线程；
带真实方法体的托管方法会摸到 Unity 的原生状态，跨线程调用就会炸。两条对策都已落地：

1. **取帧逻辑默认调度到 Unity 主线程**（`Il2Cpp.mainThread.schedule`，走 Unity 的
   `SynchronizationContext.Post`），`--no-main-thread` 可关；
2. **`isDead` 干脆不调方法** —— `Entity.get_isDead()` 和 `HealthData.isDead` 都是真实方法体，
   改为**由血量推导**（`hp <= 0`，血量本来就是裸内存读出来的明文，零 invoke，还更准）。
   想真的调一次试试，加 `--allow-is-dead`（探测里排在倒数第二，不会挡住其它步骤）。

> 判读技巧：以后要调任何一个 getter 之前，先在 dump.cs 里看它有没有
> `[CompilerGeneratedAttribute]`。没有 = 有方法体 = 必须确保在主线程上调用。

### 实测记录（2026-09-17）

第一轮：`pid=20892`、`arch=ia32`、`GameAssembly.dll=YES`，`IL2CPP ready` / `get_instance` /
`myPlayer` 全部找到，打印完 `矩阵读取模式: injected` 后进程消失，`Player.log` **没有任何异常栈**。

第二轮（用 `--level` 二分）：

| 档位 | 结果 | 结论 |
|---|---|---|
| `--level 0` | 连打 14 次 `level0 心跳` 都活着 | **不是 ACTK 检测**，frida 注入本身安全 |
| `--level 2` | `矩阵读取模式: injected`，矩阵读出且**游戏没崩** | **不是矩阵调用**，`*_Injected` 链路通了 |
| `--level 3` | 打印完 `矩阵读取模式: injected` 后 `process-terminated` | 崩在**取单例 / 遍历玩家 / 读坐标**这一层 |
| `--level 4` | 同 level 3，同一位置崩 | 与血量无关，锅在 level 3 那一层 |

第三轮（`--probe --level 3`，16 步全部打印到崩点为止）：

```
▶ [4/16]  静态字段 GameManager.myPlayer          → 0x30736720
▶ [5/16]  静态字段 Singleton<T>.<instance>k__BackingField → 0x8c2c828
▶ [6/16]  堆扫描 Il2Cpp.gc.choose(GameManager)   → 找到 1 个 → 0x8c2c828
▶ [7/16]  实例字段 gm.allPlayers                 → ? @0x2e609ea0
▶ [8/16]  allPlayers.length                      → 30
▶ [9/16]  allPlayers.get(0)                      → 0x30736720
▶ [10/16] player.get_transform()                 → 0x371412f0
▶ [11/16] transform.get_position_Injected(out)   → (7.53, 11.97, 25.24)
▶ [12/16] player.get_team()                      → GlobalRisk
▶ [13/16] player.get_isDead()   ← 崩在这里（process-terminated）
```

**结论**：取单例的两条新路（静态 backing field / 堆扫描）都成功，`allPlayers` 拿到 30 个
玩家、坐标和队伍都正常；**真凶是 `get_isDead()`** —— 它是 dump 里唯一没有
`[CompilerGenerated]` 的 getter，而 frida 线程不在 Unity 主线程上。
已按「主线程调度 + isDead 改由血量推导」修掉（见上一节 ③）。

期间还暴露了一个宿主 bug：`[frida] payload parse error: the JSON object must be str...`
疯狂刷屏，看着像挂了，其实帧数据一直是好的——见下一节。

## 排错：`payload parse error: the JSON object must be str, bytes or bytearray, not dict`

**不是游戏问题，是宿主 `on_message` 的解析 bug**（已修）。

frida 16+ 会把 `send(对象)` **直接解成 dict** 递给 `on_message`，而旧代码固定走
`json.loads(payload)`，遇到 dict 就在 20Hz 下每帧报错、把真正有用的日志全冲掉。

现在 `_coerce_payload()` 同时兼容 dict / str / bytes，并且：

- 同类解析错误**只报一次**（`WARNED_ONCE`），不再刷屏；
- `--no-overlay` 的控制台打印**节流到 2Hz**（`PRINT_THROTTLE_S`），20Hz 收帧也能看清；
- 投影失败同样只报一次并继续跑。

回归：`python tests/test_script_config.py` 里有对应的 payload 兼容用例。

## 排错：屏幕坐标离谱（`screen=(-153, 4121)`，可屏幕才 800x600）

**先说结论：投影公式是对的，不要去改 `combine_pv` / `world_to_screen`。**

2026-09-17 实测：本地玩家世界坐标 `(10.57, 13.23, -2.00)`，投影出 `(-153, 4121)`。
用纯手算（不 import `esp_core`，直接行主序 `P·V`）独立复算，结果是
`(-152.6, 4121.1)` —— **逐位吻合**。也就是说：

- `VP = P * V` 顺序正确（不是 `V * P`）；
- 列主序乘法索引 `m[col*4+row]` 正确；
- Windows 风格的 y 翻转正确；
- 同一帧的敌方坐标 `(797, 215)`、`(200, 443)`、`(166, 420)` 全在 800x600 之内，完全正常。

那 `4121` 是怎么来的？**相机就挂在本地玩家头顶 0.95 米**（第三人称）。
从 `w2c` 反推相机世界位置 `cam = -Rᵀ·t ≈ (10.688, 14.170, -2.066)`，
与玩家 `(10.57, 13.23, -2.00)` 只差 `(0.12, -0.94, 0.07)` —— 几乎是同一点。
于是透视除法的 `w = 0.1096`，`ndc_y = -12.73`，屏幕坐标被放大几十倍。
**数学上没错，但画出来毫无意义**（本地玩家本来也不需要画）。

处理办法：`esp_core` 里加了 **NDC 裁剪**（`NDC_CLIP = 3.0`）：`|ndc| > 3` 的点
标记 `clipped=True`、`on_screen=False`，叠加层跳过。注意两点：

1. **`world_to_screen` 本身不做裁剪**，仍然返回原始数学值 —— 保留它才能诊断
   （测试里专门有一条断言：`screen` 必须仍是 `(-152.6, 4121.1)`）。
2. **判据是 NDC 幅度，不是距离**。试过按距离裁剪（`dist < 1.5m`），结果合成样本
   （世界尺度是"1 单位"而非米，玩家离相机 0.65）被误裁成全部不可见。
   NDC 幅度与世界尺度无关，才是稳的判据。

打印时也会标出来，方便一眼分辨：

```
[local] screen=(-153,4121) [不可画: ndc 超界, 0.95m] team=1 hp=100/100 onscreen=False
[enemy] screen=(797,215) [23.5m] team=0 hp=100/100 onscreen=True
```

回归：`python tests/test_projection_calibration.py`（用真实对局帧做黄金断言，
含"投影数学未被篡改"和"合成样本不被误裁"两条）。

## 排错：叠加层的框位置和实际对不上

叠加层窗口**必须和游戏画面的分辨率/位置一致**，否则框会整体缩放错位。
实测游戏是 **800x600**，而旧代码硬编码 `OverlayWindow(1280, 720)` —— 会放大 1.6 倍。

**方向别搞反**：投影用的是 frame 的 `width/height`，所以**画布尺寸必须等于帧分辨率**；
游戏窗口客户区只用来提供**位置**。

现在 `overlay_tk.OverlayWindow.fit(w, h)` 每帧被调用，做两件事：

1. 画布尺寸 = 帧分辨率（游戏实测 800x600，硬编码 1280x720 会放大 1.6 倍）；
2. 位置锚点 = 游戏窗口客户区左上角：用 `FindWindowW("UnityWndClass")` +
   `GetClientRect` + `ClientToScreen` 取得（**刻意不用 EnumWindows 回调**，
   ctypes 回调在 64 位 Python 下极易 `OverflowError`，本项目已栽过一次）。

第 2 步有个必须做的校验：**只有客户区尺寸与帧分辨率严格相等（±2px）才采信它的位置**。
别退化成"宽高比一致"——实测系统里抓到过无关的 320x240 Unity 窗口，它和 800x600 的
宽高比都是 1.333，靠比例判断会把框画到错误位置。拿不到窗口就用 `(0,0)`。

锚点确认后锁定（`anchor_locked`），避免每帧抖动。

> `Screen.get_width/height()` 返回的是**渲染分辨率**。窗口化时通常等于窗口客户区；
> 若游戏开了 render scale 导致两者不等，画布仍按渲染分辨率走（与投影一致），
> 只是叠加层不会铺满窗口——坐标本身依然正确。

## 反作弊提示

你游戏内置 Anti-Cheat Toolkit（`WallHackDetector` / `InjectionDetector`）。
开发期请使用**关闭反作弊**的构建，否则 Frida 注入会被检测误报（闪退/弹窗）。
本工具定位是给自己单机游戏的调试可视化，不是绕过他人保护。

若 `--level 0` 就闪退，即可确认是注入本身被检测，此时**不要去对抗**——直接在 Unity 工程里
关掉 `InjectionDetector` / `WallHackDetector`（或用 `#if !DEVELOPMENT_BUILD` 包住初始化）重新构建。

## 后续可扩展

- 距离标注：`dist = |local.pos - p.pos|`。
- 骨骼/更精细包围盒：从 `Transform` 层次读更多关节。
- 性能：把 `overlay_tk` 换成 D3D/OpenGL 注入式 overlay（低延迟），适合最终版。
- 真机校准：拿到真实矩阵后，对照已知世界点微调 FOV / 宽高，消除投影偏差。
