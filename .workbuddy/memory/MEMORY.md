# 项目长期记忆（ucf-esp / game_for_peace_unpacker）

## 游戏与数据流
- 单机游戏 **UnityCrossFire**（U3DCF 30人生化 UCF 新年版），厂商 `Alexander_GaGa`，IL2CPP 32 位，内置 ACTK（调试须用关 ACTK 构建）。运行目录示例 `C:\aG\U3DCF单机版30人生化UCF新年版本\`。
- 链路：`frida_dump.js`(bridge 读矩阵+玩家)→send→`frida_host.py`(投影/写 shm)→`overlay_win32.exe`(D3D11 透明叠加)。**运行加载 `frida_dump.bundle.js`，改源码后必须重打包**。

## 构建 / 打包
- C++ 叠加层只在 GitHub Actions 上用 MSVC 编译（本机无 MSVC，改 C++ 后推 git 触发）；产物 `build/Release/ucf_overlay_win32.exe`。
- 打包 bundle：`cp -r /c/aB/ucf_bundle_tmp/node_modules tools/` 后
  `node esbuild/bin/esbuild frida_dump.js --bundle --format=iife --platform=neutral --charset=utf8 --outfile=frida_dump.bundle.js`，再删 node_modules。
  **`--charset=utf8` 不能省**：`platform=neutral` 默认 ASCII，会把中文转义成 `\uXXXX`，离线测试按中文锚点校验会误报「忘了重打包」。
- 帧契约 `Frame=23804B`（双缓冲 `UcfFrame`）；`MAX_PLAYERS=64, MAX_BONES=19`；`PlayerState` 含 `visible`。

## frida-il2cpp-bridge 铁律（改脚本前必读）
1. 实例方法不传 this；静态 `Klass.method().invoke()`。
2. 32 位不用结构体按值返回，走 `*_Injected(out T)`+裸内存读 float。
3. struct 字段别走 bridge 字段链（带 −headerSize 补偿）。
4. **绝不调泛型基类静态泛型方法**（`Singleton<T>.get_instance()` RVA:-1 + 隐藏 MethodInfo* → AV 进程消失）。取单例走 backing field / `Il2Cpp.gc.choose` 堆扫。
5. `Il2Cpp.Array` 无下标，用 `.get(i)`；长度加 `MAX_PLAYERS` 上限防垃圾值。
6. enum 返回 boxed 不解包：`get_team()` 改裸读 `Entity.<team>k__BackingField`@0x1C（0=BlackList 1=GlobalRisk 2=Neutral）。
7. **frida 线程≠Unity 主线程**：有真实方法体的托管方法在别处调 = AV。判读看 dump 有无 `[CompilerGeneratedAttribute]`（有=只读字段安全）。默认调度 `Il2Cpp.mainThread`+`Thread.schedule`；能推导的别调（`isDead` 由 `hp<=0` 推）。
8. **`UnityEngine.Physics` 多在 `UnityEngine.PhysicsModule`**：`camImage.class("UnityEngine.Physics")` 抛错会中断 `Il2Cpp.perform`→`startFrameLoop` 永不执行→帧不发→C++ 退回 synth 自演。须多程序集容错查找，找不到只降级 `visible=true`。
9. **【性能第一】`obj.tryMethod(name,argc)` 每次都不便宜**：`Memory.allocUtf8String(名字)` + native 方法表查找 + `bind()` 造 Proxy；取帧「每帧×每玩家×每字段」上千次 → 单帧占满主线程上百 ms（实测 8ms 采样下游戏仅 5 FPS）。已做两级缓存：`(类|名字|参数个数)`→未绑定 Method、`(类|对象|名字|参数个数)`→已绑定 Method。**禁止缓存「对象地址→类」**（对象回收后地址复用会调错方法踩野内存）。

## 关键结论
- **投影公式正确**（列主序 `VP=P·V`），勿再疑。本地玩家因第三人称相机贴头顶 0.95m 坐标爆炸属正常，应剔除；裁剪用 **NDC 幅度**（`NDC_CLIP=3.0`）非距离阈值。
- **注入后掉帧（2026-09-19 两轮排查定论）**：瓶颈在 **frida 注入的游戏进程侧**（隐藏 ESP 叠层仍卡即为证）。**先量化**：心跳 `[*] 已发送 N 帧` 间隔写死 `REPORT_MS=5000`，两次报告帧数差 ÷ 5 = 真实 FPS（实测 26/5s ≈ 5FPS，请求 8ms=125Hz ⇒ 单帧占主线程 ~190ms）。真凶按量级：① bridge 调用开销（铁律 9）② 骨骼（19×`GetBoneTransform`+失败兜底 `Transform.Find` 字符串层级搜索，实测 100% valid=false 纯浪费）③ `Physics.Linecast`。修法：方法缓存 + 骨骼/遮挡按墙钟节流(250/150ms)+按玩家缓存 + **`INTERVAL_MS` 默认 33**（采样间隔必须 ≥ 单帧真实耗时）。心跳现带 `perf:` 分项耗时行。
- **回放替代 synth**：不连游戏时 C++ 优先 `RecordedSource` 回放 `dev_frames.jsonl`（JSONL schema_version 2，循环播放，默认 `dev_replay_enabled=true`）；文件缺失才 synth。`dev_record_max_frames` 默认 500、每次录制截断为干净 clip。

## 叠加层显示（已定案）
- 4K + DPI 250% 时未声明 DPI 的进程 `GetSystemMetrics` 仅 1536×864（Tk 被放大 2.5×）→ 须在 `tk.Tk()` 前声明 DPI 感知。
- 画布=游戏客户区，4:3 进 16:9 走 **letterbox** 非 stretch（`compute_viewport` 做帧→屏映射）。
- 防闪烁三因：复用 item 不 `delete("all")`；原生 `SetWindowPos(SWP_NOACTIVATE)` 取代 `lift()`；前台判断排除自身句柄 + 0.35s 迟滞。鼠标穿透须补 `WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_NOACTIVATE`（`-transparentcolor` 只穿纯黑）。tkinter 仅创建线程可用 → 宿主只 `push()` 入队，主线程 `_tick` 消费。

## 运行环境坑
- 同名 `UnityCrossFire.exe` 常有多实例（真游戏几百 MB + 空壳），按「有 `GameAssembly.dll`」的 pid attach。
- 64 位 frida 注入 32 位 IL2CPP 正常；`taskkill` 输出 GBK 要按字节读再 `decode("gbk")`；全局热键用 `GetAsyncKeyState` 轮询（ctypes 低级键盘钩子在 64 位下 `lParam` 溢出）。
