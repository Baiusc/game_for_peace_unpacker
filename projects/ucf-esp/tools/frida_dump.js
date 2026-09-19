import "frida-il2cpp-bridge";

/*
 * UCF-ESP 运行时数据源（你自己的单机 Unity 游戏 UnityCrossFire）
 * ---------------------------------------------------------------
 * 作用：从 GameAssembly 运行时读出 相机矩阵 + 玩家坐标/血量/队伍，
 *       通过 send() 把每帧结构化数据发给宿主（见下方“宿主”说明），
 *       供调试叠加层做 world->screen 投影。
 *
 * 用法（在你本机 Windows，游戏进程已启动，或随 frida 拉起）：
 *   frida -f UnityCrossFire.exe -l tools/frida_dump.js
 *   # 或附加到已运行进程： frida -p <pid> -l tools/frida_dump.js
 *
 * 【运行档位 LEVEL】——排查“注入后游戏闪退”的关键工具
 *   宿主会在脚本前注入一行 `const UCFG = {"level":N,"interval":50};`，
 *   本文件读它决定做到哪一步。逐级往上加，哪一级开始闪退，锅就在那一层：
 *
 *     0  连 IL2CPP 都不初始化，只每 2s 打心跳
 *        -> 若这一级就闪退 = frida 注入本身被检测（ACTK 等），与读数据无关
 *     1  初始化 IL2CPP + 查类，一个方法都不 invoke
 *        -> 测 bridge 初始化 / 元数据扫描是否安全
 *     2  每帧只读相机矩阵（*_Injected out 指针），不碰 GameManager / 玩家
 *        -> 测矩阵调用（上一次就是死在这一步之后）
 *     3  + GameManager 单例 + 遍历 allPlayers + 坐标/队伍/死亡，不读血量
 *        -> 测实体遍历与坐标读取
 *     4  全量：含 ObscuredInt 血量裸读（默认档）
 *
 *   用法： python tools/frida_host.py --level 0
 *          python tools/frida_host.py --level 2
 *
 * 【单步探测 PROBE】——定位“崩在哪一次调用”
 *   宿主传 probe:true 时，注入后先按固定顺序**一步一步**执行下列调用，
 *   每步前后各打一行日志（步间 250ms，保证日志来得及送出进程）：
 *     1 Camera.get_main        2 矩阵 *_Injected    3 Screen 尺寸
 *     4 静态字段 myPlayer      5 静态字段 Singleton<T>.<instance>k__BackingField
 *     6 堆扫描 gc.choose       7 gm.allPlayers      8 length    9 get(0)
 *     10 get_transform         11 get_position_Injected
 *     12 get_team  13 get_isMyPlayer
 *     14 get_healthData + ObscuredInt 裸读（仅 level>=4）
 *     15 【高危·默认跳过】get_isDead（--allow-is-dead 才测）
 *     16 【最后·高危】GameManager.get_instance()
 *   进程若在某步炸掉，**最后那行 `▶ [n/16]` 就是崩点**。探测跑完才启动正常取帧。
 *
 * 六条硬约束（都是踩过坑换来的，改代码前先读完）：
 *
 *  1) 【实例方法不要传 this】
 *     frida-il2cpp-bridge 里 obj.method("名字", 参数个数) 返回的是**已绑定实例**的方法，
 *     .invoke(...) 只接“方法自己声明的参数”。
 *       ✅ cam.method("get_worldToCameraMatrix", 0).invoke()
 *       ❌ cam.method("get_worldToCameraMatrix").invoke(cam)
 *     后者会报：couldn't invoke method get_worldToCameraMatrix as it needs 0 parameter(s), not 1
 *
 *  2) 【32 位下不要依赖“结构体按值返回”】
 *     Matrix4x4(64B) / Vector3(12B) 按值返回在 i386 上依赖 frida 的结构体返回 ABI，
 *     容易翻车。dump.cs 里同名的 *_Injected(out T) 只要一个 out 指针，稳得多：
 *       cam.method("get_worldToCameraMatrix_Injected", 1).invoke(buf)   // void + out Matrix4x4
 *     我们给一块 Memory.alloc 当 out 缓冲，再按裸内存读，完全不碰 ABI。
 *
 *  3) 【血量是 ObscuredInt，别调值类型的方法】
 *     ObscuredInt 是 struct，插进 HealthData 里是内联布局；bridge 的 ValueType 字段访问
 *     带 header 偏移补偿，从“字段读出来的结构体”再取字段/调方法容易错位。
 *     所以直接按 dump 的偏移裸读 + ACTK 默认 XOR 解密（下面 readObscuredInt 有实证注释）。
 *
 *  4) 【别在进程里做重活】本脚本 20Hz 调用 IL2CPP，任何一步越界都可能让目标进程
 *     access violation。档位机制就是为了在出事时能把范围缩到单一调用。
 *
 *  5) 【别调泛型基类上的静态泛型方法】——这是 level3/4 硬崩溃的头号嫌疑。
 *     dump.cs 里 `Singleton<T>.get_instance()` 标着 `RVA: -1 Offset: -1`
 *     （开放泛型定义没有本地代码，只有 `Singleton<GameManager>` 的实例化版本
 *     才有真实 RVA 0x4953F0）。IL2CPP 的共享泛型方法还带一个**隐藏的 MethodInfo*
 *     参数**，按普通签名调用会让被调方拿到垃圾 MethodInfo* 并解引用 → 直接 AV。
 *     所以取单例改走两条不 invoke 的路：先读编译生成的静态 backing field
 *     `<instance>k__BackingField`，不行再 `Il2Cpp.gc.choose(GameManager)` 堆扫描。
 *     `get_instance()` 只是最后兜底（默认关闭，`--allow-get-instance` 才启用）。
 *
 *  6) 【别在 frida 线程上调“有真实方法体”的托管方法】——level3 崩在 get_isDead 的真因。
 *     frida 的 setInterval 跑在**它自己的线程**上，不在 Unity 主线程。实测规律极清晰：
 *       ✅ get_team()        —— dump 里带 [CompilerGeneratedAttribute]，只是读个字段
 *       ✅ get_transform()   —— 原生访问器
 *       ❌ Entity.get_isDead()      RVA 0x1D80B0，**没有**该特性 = 有真实方法体 → AV
 *       ⚠ Player.get_isMyPlayer()  RVA 0x2C3C80，同样没有该特性 → 同类隐患
 *     两条对策，都已落地：
 *       a) 取帧逻辑默认**调度到 Unity 主线程**执行（Il2Cpp.mainThread.schedule，
 *          走 Unity 的 SynchronizationContext.Post），`--no-main-thread` 可关；
 *       b) isDead 干脆不调方法——它和 HealthData.isDead 都是真实方法体，
 *          改为由血量推导（hp<=0，用的是裸内存读出来的明文，零 invoke）。
 *
 * 其它：
 *  - UnityEngine.Camera / Screen 在 UnityEngine.CoreModule 程序集，不在 Assembly-CSharp。
 *  - 矩阵按“列主序”展平：内存里就是 m00,m10,m20,m30,m01,… 即 m[col*4+row]，
 *    与 ucf_projection_lab 的约定一致。
 *  - 若方法名因游戏版本不同而变化，宿主传 discover:true 会打印关键类的真实导出名。
 *
 * 宿主（接收端）思路：
 *   session.on("message", (msg) => {
 *     const f = JSON.parse(msg.payload);
 *     const VP = combine_pv(f.proj, f.w2c);     // 列主序 P*V
 *     for (const p of [f.local, ...f.players]) {
 *       const s = world_to_screen(p.pos, VP, f.width, f.height);
 *       // s 即屏幕坐标，用于叠加层绘制
 *     }
 *   });
 */

// ---------------------------------------------------------------------------
// 宿主注入的运行配置（frida_host.py 会在脚本最前面拼一行 `const UCFG = {...};`）
// 直接用 `frida -l frida_dump.bundle.js` 而没有 UCFG 时，回退到 level=4 全量。
// ---------------------------------------------------------------------------
const CFG = (typeof UCFG !== "undefined" && UCFG && typeof UCFG === "object") ? UCFG : {};
const LEVEL = Number.isFinite(CFG.level) ? CFG.level : 4;
// 默认 33ms（≈30Hz）。曾经默认 8ms，但取帧是「主线程 × 每玩家 × 每字段」的一长串
// bridge 调用，8ms 这个预算根本跑不完 —— 实测单帧要 ~190ms，反而把游戏拖到 5 FPS。
// 30Hz 对 ESP 叠加与自动瞄准完全够用（人眼/屏幕刷新 60Hz，目标位置误差可忽略）。
const INTERVAL_MS = Number.isFinite(CFG.interval) ? CFG.interval : 33;
// 骨骼读取总开关与刷新周期（见 readBonesOf 上方的性能说明）
// 骨骼读取改为显式 opt-in：实机模型未确认支持 Humanoid 前，不应把失败的
// GetBoneTransform/Transform.Find 扫描放进默认取帧路径。需要骨骼时由宿主传 bones=true。
const BONES = CFG.bones === true;
const BONE_REFRESH_MS = Number.isFinite(CFG.bonesRefreshMs) ? CFG.bonesRefreshMs : 250;
// 帧内分段计时：每 5s 随心跳打一行耗时明细，用来定位「卡在哪一段」。
// CFG.perf === false 可关（省一点点开销）。
const PERF_ON = CFG.perf !== false;
const DISCOVER = CFG.discover === true;
const PROBE = CFG.probe === true;
// 默认不调用泛型基类上的静态泛型方法（见顶部约束 5），--allow-get-instance 才开
const ALLOW_GET_INSTANCE = CFG.allowGetInstance === true;
// 默认不调 get_isDead()/HealthData.isDead（真实方法体，且可由血量推导），
// --allow-is-dead 才在探测里测它（见顶部约束 6）
const ALLOW_IS_DEAD = CFG.allowIsDead === true;
// 取帧逻辑默认调度到 Unity 主线程执行（约束 6），--no-main-thread 关闭
const MAIN_THREAD = CFG.mainThread !== false;

const ASM = "Assembly-CSharp";
const UASM = "UnityEngine.CoreModule";

// C# `public static T instance { get; set; }` 编译后生成的 backing field（dump.cs 231189）
const GM_INSTANCE_FIELD = "<instance>k__BackingField";
const MAX_PLAYERS = 128; // 数组长度读到垃圾时的兜底上限，别让它带着我们去越界
const BONE_IDS = [
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  11, 12, 13, 14, 15, 16, 17, 18,
]; // Hips, legs/feet, spine/chest/neck/head, shoulders/arms/hands
const MAX_BONES = BONE_IDS.length;

const MAT_FLOATS = 16;                 // Matrix4x4：16 个 float，列主序
const MAT_BYTES = MAT_FLOATS * 4;      // 64 字节
const VEC3_BYTES = 3 * 4;              // Vector3：12 字节

const LEVEL_DESC = {
  0: "不初始化 IL2CPP，只打心跳（测：frida 注入本身是否被检测）",
  1: "初始化 IL2CPP + 查类，不 invoke 任何方法（测：bridge 初始化）",
  2: "只读相机矩阵 *_Injected（测：矩阵调用）",
  3: "+ GameManager / 玩家坐标队伍，不读血量（测：实体遍历）",
  4: "全量：含 ObscuredInt 血量裸读（默认）",
};

function discover(klass, label) {
  if (!DISCOVER) return;
  console.log(`\n[discover] ${label} (${klass.name})`);
  console.log("  methods:", klass.methods.map((m) => m.name).join(", "));
  console.log("  fields :", klass.fields.map((f) => f.name).join(", "));
}

// 这行会在脚本 load() 成功的瞬间打印，用来区分“agent 没进去”和“agent 进去了但
// 在等 IL2CPP 模块”——排查时非常重要。
console.log(`[*] frida agent loaded, waiting for IL2CPP module ... ` +
  `(level=${LEVEL}: ${LEVEL_DESC[LEVEL] || "?"}, interval=${INTERVAL_MS}ms)`);

// ===========================================================================
// LEVEL 0：完全不碰 IL2CPP。
//   如果这一级游戏就退出，说明是“frida 注入”这件事本身被检测（例如 ACTK 的
//   InjectionDetector 扫到 frida 的 agent 模块/线程），与我们的读数据逻辑无关。
// ===========================================================================
if (LEVEL <= 0) {
  let alive = 0;
  setInterval(() => {
    alive++;
    console.log(`[*] level0 心跳 ${alive}：agent 存活，未调用任何 IL2CPP API`);
  }, 2000);
} else {
Il2Cpp.perform(() => {
  console.log("[*] IL2CPP ready");

  const asmImage = Il2Cpp.domain.assembly(ASM).image;
  const camImage = Il2Cpp.domain.assembly(UASM).image;

  const Camera = camImage.class("UnityEngine.Camera");
  const Screen = camImage.class("UnityEngine.Screen");
  // UnityEngine.Physics 在多数 Unity 版本里位于 UnityEngine.PhysicsModule 程序集，
  // 而非 UnityEngine.CoreModule。直接 camImage.class("UnityEngine.Physics") 会抛
  // "couldn't find class" 异常，进而中断整个 Il2Cpp.perform 回调 —— startFrameLoop
  // 永不执行，帧从不发送，C++ 收不到帧退回 synth 自演（就是“真实帧变转圈数据”的真凶）。
  // 改为多程序集容错查找：找不到就降级（遮挡检测关闭、visible 回退 true），绝不中断初始化。
  const findPhysicsClass = () => {
    const names = ["UnityEngine.PhysicsModule", "UnityEngine.CoreModule", "UnityEngine"];
    for (const nm of names) {
      try {
        const cls = Il2Cpp.domain.assembly(nm).image.class("UnityEngine.Physics");
        if (cls) return cls;
      } catch (e) { /* 试下一个程序集 */ }
    }
    return null;
  };
  const Physics = findPhysicsClass();
  if (!Physics) console.log("[!] UnityEngine.Physics 未找到：遮挡检测关闭（visible 回退 true）");
  else console.log("[*] 遮挡检测 Physics 类已定位（" +
                   (Physics.image ? Physics.image.name : "assembly") + "）");

  // Unity 原生可见性：Renderer.isVisible 是引擎维护的可见性结果。
  // 优先取角色子树中的 SkinnedMeshRenderer，取不到时再回退到 Renderer。
  // 这是 Unity 侧成熟的可见性来源；Linecast 仅作为 Renderer 不可用时的回退。
  const findUnityClass = (name) => {
    for (const nm of ["UnityEngine.CoreModule", "UnityEngine"])
      try {
        const cls = Il2Cpp.domain.assembly(nm).image.class(name);
        if (cls) return cls;
      } catch (e) { /* 尝试下一个程序集 */ }
    return null;
  };
  const Renderer = findUnityClass("UnityEngine.Renderer");
  const SkinnedMeshRenderer = findUnityClass("UnityEngine.SkinnedMeshRenderer");
  const RENDERER_VISIBILITY = !!(Renderer || SkinnedMeshRenderer);
  if (RENDERER_VISIBILITY)
    console.log("[*] Unity Renderer.isVisible 已定位（优先 SkinnedMeshRenderer）");
  else
    console.log("[!] Unity Renderer.isVisible 未定位：可见性回退 Physics.Linecast");

  // 遮挡检测总开关 + 节流配置。优先使用 Renderer.isVisible；只有 Renderer
  // 不可取时才进入 Physics.Linecast 回退。两者都在 Unity 主线程调用，并按墙钟
  // 节流、按玩家 handle 缓存，避免把可见性查询放进每帧高频路径。
  // CFG.visibility=false 可彻底关闭（visible 恒为 true）。
  const VISIBILITY = (!!Physics || RENDERER_VISIBILITY) && CFG.visibility !== false;
  const VIS_REFRESH_MS = Number.isFinite(CFG.visRefreshMs) ? CFG.visRefreshMs : 50;
  let visCache = Object.create(null);   // handle 字符串 -> bool
  let rendererCache = Object.create(null); // player handle -> Renderer
  let visLastRefresh = -1e9;
  let visDoRefreshThisFrame = false;     // frameTick 每帧只判定一次
  if (!VISIBILITY) console.log("[*] 遮挡检测已关闭（CFG.visibility=false 且无可用 Unity 可见性 API）：visible 恒为 true");
  const GameManager = asmImage.class("GameManager");
  discover(Camera, "Camera");
  discover(GameManager, "GameManager");

  // ---------------------------------------------------------------------
  // 【性能关键】实例方法解析缓存
  //
  // bridge 的 `Object.tryMethod(name, argc)` 每次调用都要做三件不便宜的事：
  //   ① `Memory.allocUtf8String(name)` —— 一次 native 堆分配（名字必须落成 C 字符串）
  //   ② `classGetMethodFromName`       —— 一次 native 方法表字符串查找
  //   ③ `...(method).bind(this)`       —— 构造一个 JS Proxy（bind 的实现就是 new Proxy）
  // 单看一次不贵，但取帧是「每帧 × 每玩家(10) × 每个字段(≈10)」上千次调用：
  // 8ms 间隔下主线程被执行到 ~190ms/帧，游戏实测掉到 5 FPS —— 这才是“注入后卡顿”
  // 的真正大头（不是 Physics.Linecast，那条只是其中一小项）。
  //
  // 方法解析按类缓存；绑定 Method 只放在 WeakMap 对象键中，避免跨场景持有旧对象。
  // GameManager 切换时还会显式清理两级缓存。
  // ---------------------------------------------------------------------
  const METHOD_CACHE = new Map();     // (类|名字|参数个数) -> 未绑定 Method（可能为 null=确认不存在）
  // WeakMap 不持有 Unity 对象生命周期；不要用对象地址字符串作为长期缓存键。
  // Unity 换场景后地址可能复用，旧绑定不能跨对象保留。
  let BOUND_CACHE = new WeakMap();    // object -> ((类|名字|参数个数) -> 已绑定 Method)
  const CACHE_LIMIT = 20000;
  let cacheHits = 0, cacheMisses = 0;

  const clearMethodCache = () => {
    METHOD_CACHE.clear();
    BOUND_CACHE = new WeakMap();
    cacheHits = 0;
    cacheMisses = 0;
  };

  const resolveInstanceMethod = (klass, name, argc) => {
    const key = klass.handle.toString() + "|" + name + "|" + argc;
    const hit = METHOD_CACHE.get(key);
    if (hit !== undefined) return hit;              // 命中（含已确认“不存在”的 null）
    cacheMisses++;
    let m = klass.tryMethod(name, argc);            // ← 全脚本唯一的 allocUtf8String 点
    if (m && m.isStatic) m = undefined;             // 同名静态：按实例语义继续沿层级找
    if (!m) {
      // 沿继承链逐层用 native 查找（比 JS 层遍历 klass.methods 快，也避开 methods 数组重建）
      for (const k of klass.hierarchy()) {
        const cand = k.tryMethod(name, argc);
        if (cand && !cand.isStatic) { m = cand; break; }
      }
    }
    const out = m || null;
    if (METHOD_CACHE.size > CACHE_LIMIT) METHOD_CACHE.clear();
    METHOD_CACHE.set(key, out);
    return out;
  };

  const callMethod = (obj, name, argc, ...args) => {
    if (!obj) return null;
    // 注意：这里**不能**缓存「对象地址 -> 类」。Unity 会回收对象，地址复用之后旧映射
    // 会把新对象当成旧类，用错方法就可能踩到野内存。每次都现取 obj.class（一次
    // objectGetClass，native 但便宜），换来地址复用时的绝对安全 —— 类链进 key，
    // 地址被复用成别的类时自然 miss，重新解析。
    const klass = obj.class;
    const bkey = klass.handle.toString() + "|" + name + "|" + argc;
    let objectMethods = BOUND_CACHE.get(obj);
    if (!objectMethods) {
      objectMethods = new Map();
      BOUND_CACHE.set(obj, objectMethods);
    }
    let bound = objectMethods.get(bkey);
    if (bound === undefined) {
      const m = resolveInstanceMethod(klass, name, argc);
      if (!m) throw new Error(`找不到实例方法 ${name}/${argc}（含继承链）`);
      bound = m.bind(obj);
      objectMethods.set(bkey, bound);
    }
    cacheHits++;
    return bound.invoke(...args);
  };

  // 静态方法：Klass.method(名字, 参数个数).invoke(实参...)
  const callStatic = (klass, name, argc, ...args) => klass.method(name, argc).invoke(...args);

  // ---------------------------------------------------------------------
  // 主线程调度（见顶部约束 6）
  //   frida 的 setInterval 跑在 frida 自己的线程上，Unity 的托管方法（尤其是
  //   有真实方法体的那些）必须在 Unity 主线程调用，否则 AV。
  //   bridge 的 Il2Cpp.mainThread.schedule(block)：有 SynchronizationContext 时
  //   走 SendOrPostCallback + Post（返回 Promise），否则回退 Process.runOnThread。
  //   调度失败只报一次，之后永久回退到 frida 线程，避免每帧刷屏。
  // ---------------------------------------------------------------------
  const describeErr = (e) => (e && e.message) || String(e);
  const MT = { ok: null, busy: false };

  const callOnMainThread = (fn) => {
    if (!MAIN_THREAD) return fn();
    try {
      const t = Il2Cpp.mainThread;
      if (!t || typeof t.schedule !== "function") throw new Error("mainThread.schedule 不可用");
      if (MT.ok !== true) { MT.ok = true; console.log("[*] 取帧调度：Unity 主线程"); }
      return t.schedule(fn);
    } catch (e) {
      if (MT.ok !== false) {
        MT.ok = false;
        console.log("[!] 主线程调度不可用，回退到 frida 线程:", describeErr(e));
      }
      return fn();
    }
  };

  // ---------------------------------------------------------------------
  // 屏幕尺寸（用于投影；Screen.get_width/height 在 CoreModule）
  // ---------------------------------------------------------------------
  let screenCache = null;
  const screenSize = () => {
    if (screenCache) return screenCache;
    try {
      const w = callStatic(Screen, "get_width", 0);
      const h = callStatic(Screen, "get_height", 0);
      if (w > 0 && h > 0) {
        screenCache = { width: w, height: h };
        return screenCache;
      }
    } catch (e) {
      /* 忽略，用默认值 */
    }
    return { width: 1280, height: 720 };
  };

  // ---------------------------------------------------------------------
  // 相机矩阵
  // ---------------------------------------------------------------------
  const w2cBuf = Memory.alloc(MAT_BYTES);
  const projBuf = Memory.alloc(MAT_BYTES);
  const vec3Buf = Memory.alloc(VEC3_BYTES);

  const readFloats = (buf, n) => {
    const a = [];
    for (let i = 0; i < n; i++) a.push(buf.add(i * 4).readFloat());
    return a;
  };

  // 回退路径：方法按值返回时，bridge 给回的是 Il2Cpp.ValueType
  const readMatrixOf = (vt) => {
    const a = [];
    for (let c = 0; c < 4; c++)
      for (let r = 0; r < 4; r++) a.push(vt.field("m" + r + c).value);
    return a;
  };

  let matMode = null;
  const pickMatrixMode = (cam) => {
    const ok = !!cam.tryMethod("get_worldToCameraMatrix_Injected", 1) &&
               !!cam.tryMethod("get_projectionMatrix_Injected", 1);
    matMode = ok ? "injected" : "getter";
    console.log(`[*] 矩阵读取模式: ${matMode}` +
      (ok ? "（out 指针，32 位下最稳）" : "（回退：按值返回的公开 getter）"));
  };

  let visibilityOrigin = null;
  let visibilityWarned = false;
  let visibilityHitWarned = false;
  let visibilitySampleLogged = false;
  // 3rd 项目通常读取引擎维护的 spotted/render-time 布尔状态；本项目的
  // Unity 契约没有这个字段，因此保留 Physics.Linecast，但不再只测脚底根节点。
  // 根节点常在脚底，单条射线很容易先撞到目标自己的胶囊体，产生“多数黄色”。
  const VIS_ROOT_COLLIDER_TOLERANCE = 1.5;
  const VIS_BONE_COLLIDER_TOLERANCE = 0.85;
  const VISIBILITY_POINT_BONES = [11, 9, 0]; // Head, Chest, Hips（现有骨骼顺序）

  const readRendererVisibilityOf = (player) => {
    if (!RENDERER_VISIBILITY || !player) return null;
    try {
      const key = player.handle.toString();
      let renderer = rendererCache[key];
      if (!alive(renderer)) {
        const container = callMethod(player, "get_characterContainer", 0);
        if (!alive(container)) return null;
        const rendererClass = SkinnedMeshRenderer || Renderer;
        // dump.cs 确认这是 Component 的非泛型重载：GetComponentInChildren(Type,bool)。
        renderer = callMethod(container, "GetComponentInChildren", 2,
                               rendererClass.type, true);
        if (alive(renderer)) rendererCache[key] = renderer;
      }
      if (!alive(renderer)) return null;
      if (renderer.tryMethod("get_enabled", 0) &&
          callMethod(renderer, "get_enabled", 0) === false) return false;
      if (!renderer.tryMethod("get_isVisible", 0)) return null;
      return !!callMethod(renderer, "get_isVisible", 0);
    } catch (e) {
      if (!visibilityWarned) {
        visibilityWarned = true;
        console.log("[!] Renderer.isVisible 调用失败，回退 Physics.Linecast:", e.message || e);
      }
      return null;
    }
  };

  const readVisibilityOf = (player, targetTransform) => {
    if (!visibilityOrigin || !targetTransform) return null;
    const vt = perfNow();          // 记 Physics.Linecast 这一段花了多少
    try {
      const rendererVisible = readRendererVisibilityOf(player);
      if (rendererVisible === true) {
        if (!visibilitySampleLogged) {
          visibilitySampleLogged = true;
          console.log("[*] 可见性样本 source=Renderer.isVisible visible=", rendererVisible);
        }
        return true;
      }
      // isVisible=false 不是严格的视线遮挡结论：LOD、多个 Renderer、边缘裁剪
      // 都可能导致 false。交给多点 Linecast 复核，避免正常目标长期变黄色。
      if (rendererVisible === false && !visibilitySampleLogged) {
        visibilitySampleLogged = true;
        console.log("[*] Renderer.isVisible=false，转用多点 Linecast 复核遮挡");
      }
      const sx = visibilityOrigin.field("x").value;
      const sy = visibilityOrigin.field("y").value;
      const sz = visibilityOrigin.field("z").value;
      const points = [{ label: "root", transform: targetTransform,
                        tolerance: VIS_ROOT_COLLIDER_TOLERANCE }];
      // 参考成熟项目“按瞄准骨位判断可见”的做法：头/胸/髋分别取样，
      // 任一瞄准点可达即认为目标可见。这样矮墙挡住脚部时不会把整个人判成墙后。
      try {
        const animator = callMethod(player, "get_characterAnimator", 0);
        if (alive(animator) && animator.tryMethod("GetBoneTransform", 1)) {
          for (const boneIndex of VISIBILITY_POINT_BONES) {
            try {
              const bone = callMethod(animator, "GetBoneTransform", 1, BONE_IDS[boneIndex]);
              if (alive(bone)) points.push({ label: "bone" + boneIndex,
                                             transform: bone,
                                             tolerance: VIS_BONE_COLLIDER_TOLERANCE });
            } catch (e) { /* 单个骨位缺失，继续测其它点 */ }
          }
        }
      } catch (e) { /* 没有 Animator 时使用 root 回退 */ }

      let validSamples = 0;
      let invalidSamples = 0;
      let firstSample = null;
      for (const point of points) {
        const end = callMethod(point.transform, "get_position", 0);
        const ex = end.field("x").value;
        const ey = end.field("y").value;
        const ez = end.field("z").value;
        const total = Math.sqrt((ex-sx)*(ex-sx) + (ey-sy)*(ey-sy) + (ez-sz)*(ez-sz));
        const hit = Memory.alloc(0x30);
        const hitAny = callStatic(Physics, "Linecast", 5,
                                  visibilityOrigin, end, hit, -5, 0);
        if (!hitAny) {
          if (!visibilitySampleLogged) firstSample = [point.label, null, total, true];
          return true;
        }
        // UnityEngine.RaycastHit: m_FaceID=0x18, m_Distance=0x1c。
        const hitDistance = hit.add(0x1c).readFloat();
        if (!Number.isFinite(total) || total <= 0.01 ||
            !Number.isFinite(hitDistance) || hitDistance <= 0.001 ||
            hitDistance > total + 0.75) {
          invalidSamples++;
          if (!firstSample) firstSample = [point.label, hitDistance, total, null];
          continue;
        }
        validSamples++;
        const pointVisible = hitDistance >= total - point.tolerance;
        if (!firstSample) firstSample = [point.label, hitDistance, total, pointVisible];
        if (pointVisible) {
          if (!visibilitySampleLogged) {
            visibilitySampleLogged = true;
            console.log("[*] 可见性样本 point/hitDistance/total=", point.label,
                        hitDistance, "/", total, "visible=true");
          }
          return true;
        }
      }
      // 物理回写异常时 fail-open，避免 bridge 的 RaycastHit ABI 差异把全场
      // 目标变成 BLOCKED；只有至少一个有效样本且全部被提前命中才判遮挡。
      if (validSamples === 0 && invalidSamples > 0) {
        if (!visibilityHitWarned) {
          visibilityHitWarned = true;
          console.log("[!] RaycastHit 距离样本无效，visible 暂按 true:", firstSample);
        }
        return true;
      }
      if (!visibilitySampleLogged) {
        visibilitySampleLogged = true;
        console.log("[*] 可见性样本 point/hitDistance/total=", firstSample,
                    "visible=false（所有取点均被提前命中）");
      }
      return false;
    } catch (e) {
      if (!visibilityWarned) {
        visibilityWarned = true;
        console.log("[!] Physics.Linecast 不可用，visible 回退为 true:", e.message || e);
      }
      return null;
    } finally {
      if (PERF_ON) ST.visMs += Date.now() - vt;
    }
  };

  const grabMatrices = () => {
    const cam = callStatic(Camera, "get_main", 0); // 静态、无参
    if (!cam) return null;
    try {
      const ct = callMethod(cam, "get_transform", 0);
      visibilityOrigin = callMethod(ct, "get_position", 0);
    } catch (e) { visibilityOrigin = null; }
    if (matMode === null) pickMatrixMode(cam);

    if (matMode === "injected") {
      callMethod(cam, "get_worldToCameraMatrix_Injected", 1, w2cBuf);
      callMethod(cam, "get_projectionMatrix_Injected", 1, projBuf);
      return { w2c: readFloats(w2cBuf, MAT_FLOATS), proj: readFloats(projBuf, MAT_FLOATS) };
    }
    return {
      w2c: readMatrixOf(callMethod(cam, "get_worldToCameraMatrix", 0)),
      proj: readMatrixOf(callMethod(cam, "get_projectionMatrix", 0)),
    };
  };

  const logMatrix = (name, a) => {
    let s = `${name} (列主序 m[col*4+row]):\n`;
    for (let c = 0; c < 4; c++)
      s += "  col" + c + ": " +
        [0, 1, 2, 3].map((r) => a[c * 4 + r].toFixed(4).padStart(9)).join(" ") + "\n";
    console.log(s);
  };

  // ---------------------------------------------------------------------
  // GameManager（本地玩家 + 单例）
  // 关键：单例是惰性创建的，注入太早时 get_instance()/myPlayer 仍是 null。
  // 因此每帧重新读取，拿到实例之前安全跳过（不崩），不要缓存到变量里。
  // ---------------------------------------------------------------------
  try {
    console.log("[*] 找到 Singleton.get_instance:", !!GameManager.method("get_instance", 0));
  } catch (e) { console.log("[!] 找不到 get_instance:", e.message); }
  try {
    console.log("[*] 找到静态字段 myPlayer:", !!GameManager.field("myPlayer"));
  } catch (e) { console.log("[!] 找不到 myPlayer:", e.message); }

  // 三条取单例的路，按“安全度”排序。get_instance() 放最后且默认不开：
  //   ① 静态 backing field（一次静态字段读，零 invoke）
  //   ② Il2Cpp.gc.choose 堆扫描（stop-the-world，有缓存，最多 5s 一次）
  //   ③ get_instance()（泛型基类上的静态泛型方法，RVA:-1 + 隐藏 MethodInfo 参数，高危）
  let gmFromHeap = null;
  let gmHeapTs = 0;
  const alive = (o) => !!o && typeof o.isNull === "function" && !o.isNull();

  const gmFromStaticField = () => {
    try {
      const v = GameManager.field(GM_INSTANCE_FIELD).value;
      return alive(v) ? v : null;
    } catch (e) { return null; }
  };
  const gmFromHeapScan = () => {
    const now = Date.now();
    if (alive(gmFromHeap) && now - gmHeapTs < 5000) return gmFromHeap;
    gmHeapTs = now;
    try {
      const found = Il2Cpp.gc.choose(GameManager);
      gmFromHeap = found && found.length ? found[0] : null;
    } catch (e) { gmFromHeap = null; }
    return alive(gmFromHeap) ? gmFromHeap : null;
  };
  const gmFromGetInstance = () => {
    try {
      const gm = callStatic(GameManager, "get_instance", 0);
      return alive(gm) ? gm : null;
    } catch (e) { return null; }
  };

  const getGameManager = () =>
    gmFromStaticField() || gmFromHeapScan() ||
    (ALLOW_GET_INSTANCE ? gmFromGetInstance() : null);

  const getMyPlayer = () => {
    try { const v = GameManager.field("myPlayer").value; return alive(v) ? v : null; }
    catch (e) { return null; }
  };
  const getAllPlayers = (gm) => {
    try { const a = gm.field("allPlayers").value; return alive(a) ? a : null; } catch (e) { return null; }
  };

  // ---------------------------------------------------------------------
  // 坐标：Transform.get_position_Injected(out Vector3)
  // ---------------------------------------------------------------------
  let posMode = null;
  const readPosOf = (t) => {
    if (posMode === null) posMode = t.tryMethod("get_position_Injected", 1) ? "injected" : "getter";
    if (posMode === "injected") {
      callMethod(t, "get_position_Injected", 1, vec3Buf);
      return { x: vec3Buf.readFloat(), y: vec3Buf.add(4).readFloat(), z: vec3Buf.add(8).readFloat() };
    }
    const v = callMethod(t, "get_position", 0);
    return { x: v.field("x").value, y: v.field("y").value, z: v.field("z").value };
  };

  // 真实 Humanoid 骨骼：Player.characterAnimator -> Animator.GetBoneTransform(id)
  // -> Transform.get_position_Injected(out Vector3)。调用发生在 Unity 主线程，
  // 缺失骨骼只标记 valid=false，不让单个模型影响整帧。
  //
  // 【开销】每个玩家 19 次 GetBoneTransform + 19 次 readPosOf；若 GetBoneTransform
  // 全返回 null（非 Humanoid 模型常见），还要兜底跑 characterContainer.Find —— 那是
  // 按名字递归搜索整个 Transform 层级的**字符串**查找，19 槽 × 最多 3 个别名 = 最多
  // 57 次/玩家。实测骨骼 100% valid=false，也就是每帧纯浪费几百次调用在主线程上。
  // 骨架不需要 30Hz：改为按墙钟节流（默认 250ms）+ 按玩家缓存复用。
  let boneWarned = false;
  let bonesLastRefresh = -1e9;
  let bonesDoRefreshThisFrame = false;      // frameTick 每帧只判定一次
  let bonesCache = Object.create(null);     // playerHandle -> bones[]
  let bonesFailStreak = 0;                  // 连续整批无效次数（仅用于诊断日志）
  let bonesValidLast = 0;
  let bonesAutoDisabled = false;            // 本模型整批无效后，本局停止重复失败扫描
  const BONE_FAIL_LIMIT = Number.isFinite(CFG.bonesFailLimit) ? CFG.bonesFailLimit : 0;
  const BONE_NAME_ALIASES = [
    ["Hips", "Bip01 Pelvis", "mixamorig:Hips"],
    ["LeftUpLeg", "Left thigh", "Bip01 L Thigh", "mixamorig:LeftUpLeg"],
    ["RightUpLeg", "Right thigh", "Bip01 R Thigh", "mixamorig:RightUpLeg"],
    ["LeftLeg", "Left calf", "Bip01 L Calf", "mixamorig:LeftLeg"],
    ["RightLeg", "Right calf", "Bip01 R Calf", "mixamorig:RightLeg"],
    ["LeftFoot", "Bip01 L Foot", "mixamorig:LeftFoot"],
    ["RightFoot", "Bip01 R Foot", "mixamorig:RightFoot"],
    ["Spine", "Bip01 Spine", "mixamorig:Spine"],
    ["Chest", "Bip01 Spine1", "mixamorig:Spine1"],
    ["UpperChest", "Bip01 Spine2", "mixamorig:Spine2"],
    ["Neck", "Bip01 Neck", "mixamorig:Neck"],
    ["Head", "Bip01 Head", "mixamorig:Head"],
    ["LeftShoulder", "Bip01 L Clavicle", "mixamorig:LeftShoulder"],
    ["RightShoulder", "Bip01 R Clavicle", "mixamorig:RightShoulder"],
    ["LeftArm", "Bip01 L UpperArm", "mixamorig:LeftArm"],
    ["RightArm", "Bip01 R UpperArm", "mixamorig:RightArm"],
    ["LeftForeArm", "Bip01 L Forearm", "mixamorig:LeftForeArm"],
    ["RightForeArm", "Bip01 R Forearm", "mixamorig:RightForeArm"],
    ["LeftHand", "Bip01 L Hand", "mixamorig:LeftHand"],
  ];
  const findNamedBone = (container, names) => {
    if (!alive(container) || !container.tryMethod("Find", 1)) return null;
    for (const name of names) {
      try { const t = callMethod(container, "Find", 1, name); if (alive(t)) return t; }
      catch (e) { /* 当前模型没有该别名 */ }
    }
    return null;
  };
  const readBonesOf = (p) => {
    const bones = Array.from({ length: MAX_BONES }, () => ({ pos: [0, 0, 0], valid: false }));
    let validCount = 0;
    try {
      const animator = callMethod(p, "get_characterAnimator", 0);
      if (alive(animator) && animator.tryMethod("GetBoneTransform", 1)) {
        for (let i = 0; i < BONE_IDS.length; i++) {
          try {
            const t = callMethod(animator, "GetBoneTransform", 1, BONE_IDS[i]);
            if (!alive(t)) continue;
            const v = readPosOf(t);
            if (![v.x, v.y, v.z].every(Number.isFinite)) continue;
            bones[i] = { pos: [v.x, v.y, v.z], valid: true };
            validCount++;
          } catch (e) { /* 单根骨骼缺失，继续读取其余骨骼 */ }
        }
      }
      if (validCount === 0) {
        // 非 Humanoid 模型常使 GetBoneTransform 全部返回 null，改用公开 Transform.Find 兜底。
        const container = callMethod(p, "get_characterContainer", 0);
        for (let i = 0; i < BONE_NAME_ALIASES.length; i++) {
          try {
            const t = findNamedBone(container, BONE_NAME_ALIASES[i]);
            if (!alive(t)) continue;
            const v = readPosOf(t);
            if (![v.x, v.y, v.z].every(Number.isFinite)) continue;
            bones[i] = { pos: [v.x, v.y, v.z], valid: true };
          } catch (e) { /* 当前模型无该别名 */ }
        }
      }
    } catch (e) {
      if (!boneWarned) {
        boneWarned = true;
        console.log("[!] 真实骨骼读取失败，将尝试 characterContainer.Find:", e.message || e);
      }
    }
    return bones;
  };

  // ---------------------------------------------------------------------
  // 血量：ObscuredInt（CodeStage ACTK 加密整型）
  //
  // dump.cs 实证：
  //   class HealthData { ObscuredInt currentHealth; // 0x8
  //                      ObscuredInt maxHealth;     // 0x1C }
  //   struct ObscuredInt { int currentCryptoKey;  // 0x0
  //                        int hiddenValue;       // 0x4
  //                        bool inited;           // 0x8
  //                        int fakeValue;         // 0xC
  //                        bool fakeValueActive;  // 0x10 }  // 共 0x14，与 0x1C-0x8 吻合
  //
  // ACTK 默认 XOR 实现：明文 = hiddenValue ^ currentCryptoKey
  // 这里全部按裸内存读，不碰 bridge 对值类型的字段/方法封装（避免 header 偏移错位）。
  // 自检：fakeValueActive 为真时，ACTK 会把明文镜像进 fakeValue，可作交叉验证。
  // ---------------------------------------------------------------------
  let hpChecked = 0;
  const readObscuredInt = (owner, fieldName) => {
    try {
      const base = owner.handle.add(owner.field(fieldName).offset);
      const key = base.readS32();               // currentCryptoKey @0x0
      const hidden = base.add(4).readS32();     // hiddenValue      @0x4
      const inited = base.add(8).readU8();      // inited           @0x8
      const fake = base.add(12).readS32();      // fakeValue        @0xC
      const fakeActive = base.add(16).readU8(); // fakeValueActive  @0x10
      if (!inited) return null;
      const decrypted = (hidden ^ key) | 0;
      if (hpChecked < 2) { // 头两次打自检，够判断算法对不对
        hpChecked++;
        console.log(`[*] ObscuredInt 自检 ${fieldName}: hidden=0x${(hidden >>> 0).toString(16)} ` +
          `key=0x${(key >>> 0).toString(16)} -> ${decrypted}` +
          (fakeActive ? `（fakeValue=${fake} ${fake === decrypted ? "✓一致" : "✗不一致！"}）` : "（fakeValue 未启用）"));
      }
      // 明显不合理的值（负 / 超大）说明算法或偏移对不上，宁可给 null 也别画错
      if (decrypted < 0 || decrypted > 100000) return null;
      return decrypted;
    } catch (e) { return null; }
  };

  // ---------------------------------------------------------------------
  // 队伍（Team 是 enum : int，dump 231002：0=BlackList 1=GlobalRisk 2=Neutral）
  //
  // 坑（实测）：get_team() 虽然是 [CompilerGenerated] 的字段直读、调用本身安全，
  // 但 bridge 把返回值当成 **boxed Object**（没解包 enum 的 value__），
  // 传到宿主是 {"handle":..., "type":...} —— 两个对象永远不相等，
  // 于是“队友 / 敌人”判定彻底失效（全员被当成敌人）。
  // 所以改成直接读 backing field 的原始 int：零 invoke，且能直接比较。
  // ---------------------------------------------------------------------
  const TEAM_FIELD = "<team>k__BackingField";   // Entity 私有字段，dump 223578
  const TEAM_OFFSET_FALLBACK = 0x1c;            // 同上，bridge 找不到名字时的兜底
  const readTeamOf = (p) => {
    try {
      const v = p.handle.add(p.field(TEAM_FIELD).offset).readS32();
      return Number.isFinite(v) ? v : null;
    } catch (e) {}
    try {
      const v = p.handle.add(TEAM_OFFSET_FALLBACK).readS32();
      return Number.isFinite(v) ? v : null;
    } catch (e) { return null; }
  };

  // ---------------------------------------------------------------------
  // 单个玩家（withHp=false 时跳过一切 ObscuredInt 裸读，用于 LEVEL 3 分档）
  // ---------------------------------------------------------------------
  const readPlayer = (p, withHp) => {
    const t0 = perfNow();          // perfNow/ST 定义在下方但此处是运行时调用，安全
    const out = { isLocal: false };
    try {
      const t = callMethod(p, "get_transform", 0);
      out.pos = t ? readPosOf(t) : null;
      if (!VISIBILITY || !t) {
        out.visible = null;                 // 关闭遮挡检测 / 无 transform：全部视为可见
      } else if (visDoRefreshThisFrame) {
        // 真正的主线程 Physics.Linecast，仅在“重算帧”执行（每 ~150ms 一次）。
        // 血量字段在本函数后段才读取；这里不能提前用 out.hp 做死亡短路，
        // 否则该条件永远是 undefined，既没有节省射线，也会误导后续维护。
        out.visible = readVisibilityOf(p, t);
        visCache[t.handle.toString()] = out.visible;
      } else {
        // 非重算帧：复用上次缓存，避免每帧 30 次物理射线拖垮游戏 FPS。
        const k = t.handle.toString();
        out.visible = (k in visCache) ? visCache[k] : null;
      }
    } catch (e) { out.pos = null; }
    out.team = readTeamOf(p);
    try { out.isMyPlayer = callMethod(p, "get_isMyPlayer", 0); } catch (e) { out.isMyPlayer = null; }
    out.isLocal = out.isMyPlayer === true;
    if (withHp) {
      try {
        const hd = callMethod(p, "get_healthData", 0);
        out.hp = hd ? readObscuredInt(hd, "currentHealth") : null;
        out.maxHp = hd ? readObscuredInt(hd, "maxHealth") : null;
      } catch (e) { out.hp = null; out.maxHp = null; }
    }
    // 骨骼：只在「重算帧」真正读取（每 ~250ms 一次），其余帧复用缓存。
    if (!BONES || bonesAutoDisabled || !withHp) {
      out.bones = [];
    } else if (bonesDoRefreshThisFrame) {
      const bt = perfNow();
      out.bones = readBonesOf(p);
      bonesCache[p.handle.toString()] = out.bones;
      if (PERF_ON) ST.boneMs += Date.now() - bt;
      ST.boneReads++;
      for (let i = 0; i < out.bones.length; i++) if (out.bones[i].valid) bonesValidLast++;
    } else {
      out.bones = bonesCache[p.handle.toString()] || [];
    }
    // isDead：Entity.get_isDead() 和 HealthData.isDead 都是“有真实方法体”的
    // 非 trivial getter（dump 里没有 [CompilerGeneratedAttribute]），默认不调。
    // 改由血量推导 —— 血量本来就是裸内存读出来的明文，零 invoke，更安全也更准。
    if (ALLOW_IS_DEAD) {
      try { out.isDead = callMethod(p, "get_isDead", 0); } catch (e) { out.isDead = null; }
    } else {
      out.isDead = out.hp === undefined || out.hp === null ? null : out.hp <= 0;
    }
    if (PERF_ON) { ST.playerMs += Date.now() - t0; ST.players++; }
    return out;
  };

  // =====================================================================
  // LEVEL 1：IL2CPP 已初始化、类也找到了，到此为止——不 invoke 任何方法。
  //   若这一级闪退，问题在 bridge 的初始化/元数据扫描，不在我们的调用。
  // =====================================================================
  if (LEVEL <= 1) {
    let hb = 0;
    setInterval(() => {
      hb++;
      console.log(`[*] level1 心跳 ${hb}：IL2CPP 已初始化，未调用任何实例方法`);
    }, 2000);
    return;
  }

  // ---------------------------------------------------------------------
  // 每帧快照（动态读取，null 安全）
  // ---------------------------------------------------------------------
  let tick = 0;
  let sent = 0;
  let everHadGm = false;   // 曾经拿到过 GameManager 实例 => 现在没了就是退出对局
  let lastGameManagerHandle = null;
  let lastWarn = 0;        // 用时间戳节流告警，避免高频刷屏
  let lastReport = 0;
  let lastErr = 0;
  const WARN_MS = 3000;
  const REPORT_MS = 5000;

  // 帧内分段计时（毫秒累计，报告后清零）：用来量出“卡”到底卡在哪一段。
  // Date.now() 只有 1ms 精度，但这里是「一帧内累积几十次调用」的总和，够用。
  const ST = {
    frames: 0, players: 0,
    tickMs: 0, playerMs: 0, boneMs: 0, visMs: 0, sendMs: 0, boneReads: 0,
  };
  let tickT0 = 0;
  const perfNow = () => (PERF_ON ? Date.now() : 0);

  const perfReport = () => {
    if (!PERF_ON || ST.frames === 0) return "";
    const n = ST.frames;
    const avg = (v) => (v / n).toFixed(1);
    return ` | perf: 单帧 ${avg(ST.tickMs)}ms（玩家 ${avg(ST.playerMs)}` +
           ` / 骨骼 ${avg(ST.boneMs)}·${ST.boneReads}次` +
           ` / 遮挡 ${avg(ST.visMs)} / send ${avg(ST.sendMs)}）` +
           ` 均玩家 ${(ST.players / n).toFixed(1)}` +
           ` 调用命中 ${cacheHits}/缺失 ${cacheMisses}` +
           ` 实际 ${(n * 1000 / REPORT_MS).toFixed(1)}FPS`;
  };
  const perfReset = () => {
    ST.frames = ST.players = 0;
    ST.tickMs = ST.playerMs = ST.boneMs = ST.visMs = ST.sendMs = ST.boneReads = 0;
  };

  // 骨骼一条都读不到时给出明确诊断 —— 否则用户只看到“没有骨架”却不知为何，
  // 会误以为是投影/叠加层的问题。
  let bonesDiagCount = 0;
  const reportBonesDiag = () => {
    if (!BONES || bonesLastRefresh < 0) return;      // 没启用 / 还没读过一次
    if (bonesValidLast > 0) { bonesDiagCount = 0; return; }
    if (bonesDiagCount === 0 || bonesDiagCount % 6 === 0) {
      console.log("[!] 骨骼读取全部无效：角色模型可能不是 Humanoid，或骨骼名不在脚本的" +
                  "BONE_IDS/别名表里 —— 骨架显示不出来与此有关，不是投影问题。" +
                  "（已按 " + BONE_REFRESH_MS + "ms 节流，不再拖累帧率；--no-bones 可彻底关）");
    }
    bonesDiagCount++;
  };

  // 不在对局 / 还没相机 时也要照常发帧（inGame:false）：
  // 以前直接 return，宿主既看不到"我还活着"（会误报 10s 无数据），
  // 又不会清屏 —— 回菜单后屏幕上一直留着上一次的框。
  const sendStatus = (reason) => {
    let scr = { width: 0, height: 0 };
    try { scr = screenSize(); } catch (e) { /* 拿不到就算了 */ }
    send({ type: "frame", tick, inGame: false, reason,
           width: scr.width, height: scr.height, local: null, players: [] });
  };

  // 单帧逻辑（会被调度到 Unity 主线程执行，见 callOnMainThread）
  const frameTick = () => {
    tick++;
    const now = Date.now();
    if (PERF_ON) tickT0 = now;

    // 遮挡检测节流：每 ~VIS_REFRESH_MS 整批重算一次，整帧只判定一次，结果按玩家缓存复用。
    if (VISIBILITY) {
      if ((now - visLastRefresh) >= VIS_REFRESH_MS) {
        visCache = Object.create(null);
        visLastRefresh = now;
        visDoRefreshThisFrame = true;
      } else {
        visDoRefreshThisFrame = false;
      }
    } else {
      visDoRefreshThisFrame = false;
    }

    // 骨骼节流：同理，每 ~BONE_REFRESH_MS 整批读一次（骨架 4Hz 足够，省主线程）。
    if (BONES && LEVEL >= 4) {
      if ((now - bonesLastRefresh) >= BONE_REFRESH_MS) {
        bonesCache = Object.create(null);
        bonesLastRefresh = now;
        bonesDoRefreshThisFrame = true;
        bonesValidLast = 0;      // 本帧重新累计有效骨骼数（诊断用）
      } else {
        bonesDoRefreshThisFrame = false;
      }
    } else {
      bonesDoRefreshThisFrame = false;
    }

    let M;
    try {
      M = grabMatrices();
    } catch (e) {
      if (now - lastErr > WARN_MS) { lastErr = now; console.log("[!] 读矩阵失败:", e.message || e); }
      sendStatus("matrix-error");
      return;
    }
    if (!M) {
      if (now - lastWarn > WARN_MS) { console.log("[!] 等待主相机 Camera.main ..."); lastWarn = now; }
      sendStatus("no-camera");
      return;
    }

    const scr = screenSize();

    // LEVEL 2：只交矩阵，不碰 GameManager / 玩家，便于单独验证矩阵调用
    if (LEVEL <= 2) {
      send({ type: "frame", tick, width: scr.width, height: scr.height,
             w2c: M.w2c, proj: M.proj, local: null, players: [] });
      sent++;
      if (tick <= 3) {
        logMatrix("worldToCameraMatrix", M.w2c);
        logMatrix("projectionMatrix", M.proj);
        console.log("[frame/level2]", JSON.stringify({ w2c: M.w2c, proj: M.proj }));
      } else if (now - lastReport > REPORT_MS) {
        lastReport = now;
        console.log(`[*] level2 已发送 ${sent} 帧（只读矩阵）`);
      }
      return;
    }

    const gm = getGameManager();
    if (!gm) {
      if (lastGameManagerHandle !== null) {
        clearMethodCache();
        lastGameManagerHandle = null;
        bonesCache = Object.create(null);
        visCache = Object.create(null);
        rendererCache = Object.create(null);
        bonesAutoDisabled = false;
      }
      // 曾经拿到过又没了 = 退出对局回菜单（实例被销毁），不是“还没创建”，别再干等
      if (now - lastWarn > WARN_MS) {
        console.log(everHadGm
          ? "[*] 不在对局中（GameManager 实例已销毁，进对局后自动恢复）..."
          : "[*] 等待 GameManager 实例（尚未创建，进对局后再按 INS 注入更省事）...");
        lastWarn = now;
      }
      sendStatus(everHadGm ? "left-match" : "no-game-manager");
      return;
    }
    everHadGm = true;
    const gmHandle = gm.handle.toString();
    if (lastGameManagerHandle !== null && lastGameManagerHandle !== gmHandle) {
      clearMethodCache();
      bonesCache = Object.create(null);
      visCache = Object.create(null);
      rendererCache = Object.create(null);
      bonesAutoDisabled = false;
      console.log("[*] GameManager 已切换：清理对象方法缓存");
    }
    lastGameManagerHandle = gmHandle;

    const myPlayer = getMyPlayer();
    const allPlayers = getAllPlayers(gm);

    const frame = {
      type: "frame",
      tick,
      inGame: true,
      width: scr.width,
      height: scr.height,
      w2c: M.w2c,
      proj: M.proj,
      local: myPlayer ? readPlayer(myPlayer, LEVEL >= 4) : null,
      players: [],
    };
    if (allPlayers) {
      // 注意：bridge 的 Il2Cpp.Array 没有下标代理，只有 .get(i)（直接 allPlayers[i]
      // 永远是 undefined —— 之前玩家列表恒空就是这个坑）。
      let n = 0;
      try { n = allPlayers.length || 0; } catch (e) { n = 0; }
      const useGet = typeof allPlayers.get === "function";
      if (n > MAX_PLAYERS) { // 长度读到垃圾值：宁可截断也别带着它去越界
        if (now - lastWarn > WARN_MS) {
          lastWarn = now;
          console.log(`[!] allPlayers.length=${n} 异常，截断为 ${MAX_PLAYERS}`);
        }
        n = MAX_PLAYERS;
      }
      for (let i = 0; i < n; i++) {
        let p = null;
        try { p = useGet ? allPlayers.get(i) : allPlayers[i]; } catch (e) { p = null; }
        if (alive(p)) frame.players.push(readPlayer(p, LEVEL >= 4));
      }
      if (tick <= 3) {
        console.log(`[*] allPlayers length=${n} 有效=${frame.players.length}` +
          `（null 槽位 ${n - frame.players.length}）取元素=${useGet ? ".get(i)" : "下标"}`);
      }
    }
    if (BONES && bonesDoRefreshThisFrame && bonesValidLast === 0) {
      bonesAutoDisabled = true;
      console.log("[!] 本局骨骼读取全部无效，停止重复骨骼扫描；需要重新探测请切换对局或显式重启脚本");
    }
    const st = perfNow();
    send(frame); // 发给宿主做投影 / 叠加
    sent++;
    if (PERF_ON) {
      ST.sendMs += Date.now() - st;
      ST.tickMs += Date.now() - tickT0;
      ST.frames++;
    }
    if (tick <= 3) {
      logMatrix("worldToCameraMatrix", M.w2c);
      logMatrix("projectionMatrix", M.proj);
      console.log("[frame]", JSON.stringify(frame));
    } else if (now - lastReport > REPORT_MS) {
      lastReport = now; // 心跳：证明循环还活着（叠加层看不到时可据此判断）
      console.log(`[*] 已发送 ${sent} 帧 (level=${LEVEL}, players=${frame.players.length}, screen=${scr.width}x${scr.height})`
                  + perfReport());
      reportBonesDiag();
      perfReset();
    }
  };

  // 心跳定时器留在 frida 线程（它只负责“到点了”），真正的 IL2CPP 调用交给主线程。
  // MT.busy 防止上一帧没跑完就叠下一帧（主线程 Post 是异步的，会堆积）。
  const startFrameLoop = () => setInterval(() => {
    if (MT.busy) return;
    MT.busy = true;
    const release = () => { MT.busy = false; };
    try {
      const r = callOnMainThread(frameTick);
      if (r && typeof r.then === "function") r.then(release, (e) => {
        release();
        console.log("[!] 主线程取帧失败:", describeErr(e));
      });
      else release();
    } catch (e) {
      release();
      console.log("[!] 取帧异常:", describeErr(e));
    }
  }, INTERVAL_MS);

  // =====================================================================
  // 单步探测：一次运行把所有关键调用各做一遍，崩在哪一步一目了然。
  // 顺序刻意把“高危的 get_instance()”放在最后，即使它炸了前面的结论也已拿到。
  // 用 setInterval 串成状态机（不用 async/await），步间 250ms 让日志来得及送出。
  // =====================================================================
  const buildProbeSteps = () => {
    const P = {};
    const hp = (o) => (alive(o) ? o.handle.toString() : "null");
    return [
      ["Camera.get_main()", () => { P.cam = callStatic(Camera, "get_main", 0); return hp(P.cam); }],
      ["矩阵 *_Injected(out)", () => {
        if (!alive(P.cam)) return "跳过（无相机）";
        callMethod(P.cam, "get_worldToCameraMatrix_Injected", 1, w2cBuf);
        callMethod(P.cam, "get_projectionMatrix_Injected", 1, projBuf);
        return "w2c[0..3]=" + readFloats(w2cBuf, 16).slice(0, 4).map((v) => v.toFixed(2)).join(",");
      }],
      ["Screen.get_width/height", () => { const s = screenSize(); return `${s.width}x${s.height}`; }],
      ["静态字段 GameManager.myPlayer", () => { P.my = getMyPlayer(); return hp(P.my); }],
      [`静态字段 Singleton<T>.${GM_INSTANCE_FIELD}`, () => { P.f1 = gmFromStaticField(); return hp(P.f1); }],
      ["堆扫描 Il2Cpp.gc.choose(GameManager)", () => {
        // 直接走生产代码里那个函数，顺便报告堆上有几个实例
        const a = Il2Cpp.gc.choose(GameManager);
        P.f2 = gmFromHeapScan();
        return `找到 ${a ? a.length : 0} 个 → ${hp(P.f2)}`;
      }],
      ["实例字段 gm.allPlayers", () => {
        P.gm = P.f1 || P.f2;
        if (!alive(P.gm)) return "跳过（没有 GameManager 实例）";
        P.arr = getAllPlayers(P.gm);
        return alive(P.arr) ? `${P.arr.class ? P.arr.class.name : "?"} @${hp(P.arr)}` : "null";
      }],
      ["allPlayers.length", () => (alive(P.arr) ? String(P.arr.length || 0) : "跳过")],
      ["allPlayers.get(0)", () => {
        if (!alive(P.arr) || !(P.arr.length > 0)) return "跳过（空数组）";
        P.p0 = P.arr.get(0);
        return hp(P.p0);
      }],
      ["player.get_transform()", () => {
        if (!alive(P.p0)) return "跳过";
        P.t0 = callMethod(P.p0, "get_transform", 0);
        return hp(P.t0);
      }],
      ["transform.get_position_Injected(out)", () => {
        if (!alive(P.t0)) return "跳过";
        const v = readPosOf(P.t0);
        return `(${v.x.toFixed(2)}, ${v.y.toFixed(2)}, ${v.z.toFixed(2)})`;
      }],
      ["player 队伍：裸读 int vs get_team()", () => {
        if (!alive(P.p0)) return "跳过（无玩家对象）";
        const raw = readTeamOf(P.p0);
        let obj = null;
        try { obj = callMethod(P.p0, "get_team", 0); } catch (e) { obj = `抛异常: ${(e && e.message) || e}`; }
        const shown = (obj && typeof obj === "object")
          ? `对象(handle=${obj.handle}) ← bridge 不解包 enum，宿主无法比较`
          : String(obj);
        return `裸读=${raw} [0=BlackList 1=GlobalRisk 2=Neutral] / get_team()=${shown}`;
      }],
      ["player.get_isMyPlayer()", () => (alive(P.p0) ? String(callMethod(P.p0, "get_isMyPlayer", 0)) : "跳过")],
      ["get_healthData() + ObscuredInt 裸读", () => {
        if (LEVEL < 4) return "跳过（level<4，本次不读血量）";
        if (!alive(P.p0)) return "跳过（无玩家对象）";
        const hd = callMethod(P.p0, "get_healthData", 0);
        if (!alive(hd)) return "healthData=null";
        return `hp=${readObscuredInt(hd, "currentHealth")} / max=${readObscuredInt(hd, "maxHealth")}`;
      }],
      // get_isDead 是“有真实方法体”的非 trivial getter（约束 6），level3 就崩在它身上。
      // 默认跳过，--allow-is-dead 才真的调一次（此时已调度到主线程，理论上安全）。
      ["【高危·默认跳过】player.get_isDead()", () => {
        if (!ALLOW_IS_DEAD) return "跳过（--allow-is-dead 才测；默认改由 hp<=0 推导）";
        if (!alive(P.p0)) return "跳过（无玩家对象）";
        return String(callMethod(P.p0, "get_isDead", 0));
      }],
      ["【最后·高危】GameManager.get_instance()", () => {
        P.f3 = gmFromGetInstance();
        return hp(P.f3) + (ALLOW_GET_INSTANCE ? "" : "（默认路径不会调它）");
      }],
    ];
  };

  const runProbe = (done) => {
    const steps = buildProbeSteps();
    const total = steps.length;
    const STEP_TIMEOUT_MS = 3000; // 主线程 Post 是异步的：迟迟不回就报超时，别把探测卡死
    let i = 0;
    let pending = false;
    let stepTs = 0;
    console.log(`[*] 单步探测开始：共 ${total} 步，每步间隔 250ms（崩了看最后一行 ▶）`);
    const t = setInterval(() => {
      if (pending) {
        // 还在等上一步的结果（可能被调度到主线程去了）
        if (Date.now() - stepTs > STEP_TIMEOUT_MS) {
          console.log(`   ⚠ 上一步超过 ${STEP_TIMEOUT_MS}ms 未返回（主线程可能没在泵消息），继续`);
          pending = false;
        }
        return;
      }
      if (i >= total) {
        clearInterval(t);
        console.log("[*] 单步探测全部完成，开始正常取帧");
        done();
        return;
      }
      const name = steps[i][0];
      const fn = steps[i][1];
      i++;
      console.log(`▶ [${i}/${total}] ${name} ...`);
      stepTs = Date.now();
      let r;
      try {
        r = callOnMainThread(fn);
      } catch (e) {
        console.log(`   ✗ ${name} 抛异常: ${describeErr(e)}`);
        return;
      }
      if (r && typeof r.then === "function") {
        pending = true;
        r.then(
          (v) => { console.log(`   ✓ ${name} → ${v}`); pending = false; },
          (e) => { console.log(`   ✗ ${name} 失败: ${describeErr(e)}`); pending = false; }
        );
      } else {
        console.log(`   ✓ ${name} → ${r}`);
      }
    }, 250);
  };

  if (PROBE) runProbe(startFrameLoop);
  else startFrameLoop();
});
}
