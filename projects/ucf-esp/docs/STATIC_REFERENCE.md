# UCF-ESP 静态参考（dump.cs 实证）

> 目标：给你自己用 Unity 制作的单机游戏 **UnityCrossFire**（U3DCF 单机版 30 人生化新年版）做调试 / 可视化数据源。
> 这是给自己游戏写的开发期调试工具，**不是在线对战作弊**。
> 投影实验室 `ucf_projection_lab` 保持合成数据、离线运行；本文档只负责把
> “运行时读什么、从哪里读”定下来，并和实验室的输入格式对齐。

## 0. 反作弊提示（重要）

`dump.cs` 显示游戏内含 **Anti-Cheat Toolkit**：

- `ObscuredInt` 等加密数值类型（血量字段就是它）→ 直接读字段是密文
- 检测器：`WallHackDetector`(220560)、`InjectionDetector`(219570)、
  `ObscuredCheatingDetector`(219638)、`SpeedHackDetector`(219733)、
  `TimeCheatingDetector`(220204)

用 Frida 注入调试时，注入类检测器可能**误报**。开发期建议：
用**关闭反作弊**的构建，或在可信/单机环境里调试。

---

## 1. 矩阵来源（相机）

| 项 | 名称 | 说明 |
|---|---|---|
| 程序集 | `UnityEngine.CoreModule` | Camera 不在这个游戏的 Assembly-CSharp，而在 UnityEngine 核心模块 |
| 主相机入口（静态） | `UnityEngine.Camera.get_main()` | 返回 Camera 实例（dump.cs 111452） |
| 视图矩阵 | `Camera.get_worldToCameraMatrix()` | 返回 `Matrix4x4`（dump.cs 111409） |
| 投影矩阵 | `Camera.get_projectionMatrix()` | 返回 `Matrix4x4`（dump.cs 111415） |
| 原生版本（**实际采用**） | `get_worldToCameraMatrix_Injected(out Matrix4x4)` / `get_projectionMatrix_Injected(out Matrix4x4)` | 用 out 参数（dump.cs 111542 / 111548） |

> **为什么实际用 `_Injected` 而不是公开 getter：** `Matrix4x4` 是 64 字节值类型，公开 getter 是
> **按值返回**；在 32 位（i386）下这叫依赖 frida 的结构体返回 ABI，属于易翻车路径。
> `_Injected(out Matrix4x4 ret)` 的签名是 `void (this, Matrix4x4* ret)`，我们给一块
> `Memory.alloc(64)` 当 out 缓冲、再按裸内存读 16 个 float，完全不碰 ABI。
> `frida_dump.js` 会先探测 `_Injected` 是否存在，缺失时才退回公开 getter（并在日志里说明）。
> 同理，玩家坐标走 `Transform.get_position_Injected(out Vector3)`（12 字节）。

`Matrix4x4` 字段为 `m00..m33`，内存为**列主序**：`m00,m10,m20,m30,m01,m11,...`
即扁平索引 `m[col*4+row]`。

> 列主序有实证：dump.cs 里 `Matrix4x4` 的字段**声明顺序**就是
> `m00,m10,m20,m30,m01,...`，偏移 `0x0,0x4,...0x3C` 连续 ——
> 前 4 个 float 恰好是第 0 列。`tests/test_dump_contract.py` 会持续校验这一点。

---

## 2. 本地玩家

| 来源 | 说明 |
|---|---|
| **`GameManager.myPlayer`**（推荐） | `public static Player myPlayer; // 0x0`（dump.cs 230778），静态字段，无需实例即可读 |
| 备用逐玩家标志 | `Player.get_isMyPlayer()`（dump.cs 0x2C3C80） |

`GameManager` 是 `Singleton<GameManager>`，单例实例见第 3 节。

---

## 3. 实体列表（全玩家）

`GameManager` 实例字段（dump.cs 230783–230787）：

| 字段 | 偏移 | 类型 |
|---|---|---|
| `allPlayers` | 0x14 | `Player[]` |
| `playersBL` | 0x18 | `List<Player>` |
| `playersBL_Alive` | 0x1C | `List<Player>` |
| `playersGR` | 0x20 | `List<Player>` |
| `playersGR_Alive` | 0x24 | `List<Player>` |

### 取 `GameManager` 实例：**不要调 `get_instance()`**（重要）

`get_instance()` 声明在泛型基类 `Singleton<T>` 上（dump.cs 231205），dump 里标着：

```csharp
[CompilerGeneratedAttribute] // RVA: -1 Offset: -1
public static T get_instance() { }
```

即**开放泛型定义没有本地代码**，只有 `Singleton<GameManager>` 的实例化版本才有真实
RVA `0x4953F0`；而且 IL2CPP 的共享泛型方法还带一个**隐藏的 `MethodInfo*` 参数**，
按普通签名调用会让被调方拿到垃圾 `MethodInfo*` 并解引用 → **access violation，
进程瞬间消失且 `Player.log` 里没有异常栈**（2026-09-17 实测：level3/4 就死在这）。

改为两条**不 invoke 任何方法**的路（按序尝试）：

1. 读编译生成的静态 backing field `Singleton<T>.<instance>k__BackingField`
   （`public static T instance { get; set; }` 自动生成，dump.cs 231189）——一次静态字段读；
2. 不行再 `Il2Cpp.gc.choose(GameManager)` 堆扫描（带 5s 缓存）。

`get_instance()` 仅作最后兜底，`frida_dump.js` 里默认关闭（`--allow-get-instance` 才开）。

> 遍历用 `allPlayers`（`Player[]`）：bridge 的 `Il2Cpp.Array` **只有 `.length` / `.get(i)`，
> 没有 `arr[i]` 下标代理**，写 `allPlayers[i]` 会恒为 `undefined`（静默空列表）。
> 分阵营列表 `playersBL/GR` 用于按队伍筛选。

---

## 4. Player / Entity 关键字段偏移

坐标**不在 Player 直接字段**里 —— 要走 `Transform`（原生访问器）。

| 类 | 字段 / 属性 | 偏移 | 类型 | 读法 |
|---|---|---|---|---|
| Entity | `healthData` | 0x18 | `HealthData` | `get_healthData()` |
| Entity | `team` | 0x1C | `Team` | `get_team()` |
| Entity | `isDead`（属性） | — | `bool` | `get_isDead()` |
| Player | `characterContainer` | 0x4C | `Transform` | `get_characterContainer()`（模型挂载点） |
| Entity | `characterAnimator` | 0x24 | `Animator` | `get_characterAnimator()` |
| Player（继承 MonoBehaviour） | 世界坐标 | — | `Vector3` | `get_transform().get_position()` |
| Player | `velocity` | 0x80 | `Vector3` | `get_...` / 直接字段 |
| Player | `spawnPos` | 0xAC | `Vector3` | `get_spawnPos()`（参考点） |

`HealthData`（dump.cs 222987）字段：

| 字段 | 偏移 | 类型 | 备注 |
|---|---|---|---|
| `currentHealth` | 0x8 | `ObscuredInt` | 加密，裸内存 XOR 解密（第 5 节） |
| `maxHealth` | 0x1C | `ObscuredInt` | 加密，裸内存 XOR 解密（第 5 节） |
| `tempHealth` | 0x30 | `ObscuredInt` | 加密 |
| `isDead` | — | `bool` | `get_isDead()` |
| `invinsibleEndTime` | 0x44 | `float` | 无敌结束时间（明文，可直接读） |

> 字段偏移是**相对对象起始**（含 8 字节 `System.Object` 头），
> 所以裸读时的地址是 `healthDataObj.handle + 0x8`。

### 4.1 真实 Humanoid 骨骼

本游戏 dump 中 `Animator.GetBoneTransform(HumanBodyBones)` 存在，枚举值来自
`UnityEngine.HumanBodyBones`：`Hips=0`、`Spine=7`、`Chest=8`、`Neck=9`、
`Head=10`、肩/臂/手为 `11..18`、腿/脚为 `1..6`。每帧读取链为：

```text
Player.get_characterAnimator()
  -> Animator.GetBoneTransform(enum id)
  -> Transform.get_position_Injected(out Vector3)
```

Frida 固定输出 19 个槽位；不存在的 Humanoid 节点输出 `valid=false`。取帧已经在
Unity 主线程调度中，因此 `GetBoneTransform` 与位置访问不会从 Frida 定时器线程直接调用。

---

## 5. `ObscuredInt` 解密

- 类型：`public struct ObscuredInt`（dump.cs 217509），是 CodeStage ACTK 的加密整型。
- 布局（dump.cs 实证，全部是**结构体内 0 基偏移**）：

| 字段 | 偏移 | 类型 | 说明 |
|---|---|---|---|
| `currentCryptoKey` | 0x0 | `int` | 本实例的 XOR 密钥 |
| `hiddenValue` | 0x4 | `int` | 密文 |
| `inited` | 0x8 | `bool` | 未初始化时读到的值无意义 |
| `fakeValue` | 0xC | `int` | 明文镜像（`fakeValueActive` 为真时才有意义） |
| `fakeValueActive` | 0x10 | `bool` | 镜像是否启用 |

结构体大小 `0x14`，与 `HealthData` 里 `currentHealth(0x8) → maxHealth(0x1C)` 的间距完全吻合。

- **明文 = `hiddenValue ^ currentCryptoKey`**（ACTK 默认 XOR 实现）。
- 本工具的实际取法：**裸内存读**，不调方法：

```js
const base     = hd.handle.add(hd.field("currentHealth").offset); // HealthData + 0x8
const key      = base.readS32();          // currentCryptoKey @0x0
const hidden   = base.add(4).readS32();   // hiddenValue      @0x4
const inited   = base.add(8).readU8();    // inited           @0x8
if (!inited) return null;
const hp = (hidden ^ key) | 0;
```

> **为什么不去调 `InternalDecrypt()` / `GetDecrypted()`：**
> 它们都定义在**值类型** `ObscuredInt` 上，而 bridge 对值类型的字段访问/方法绑定带一层
> `−headerSize` 的偏移补偿（`Field.bind` / `Method.bind` 里可见）。从「字段读出来的
> `Il2Cpp.ValueType`」再取字段或调方法，指针基准容易差一个 header → 读到乱值甚至触发访问违例。
> 裸内存读没有这些隐含前提，且能用 `fakeValue` 做交叉验证。
>
> 交叉验证：`fakeValueActive` 为真时，ACTK 会把明文镜像进 `fakeValue`；
> 若 `fakeValue === (hidden ^ key)` 即证明算法与偏移都正确。
> `frida_dump.js` 会在启动的前两次读取时把这两个值一起打印出来。

---

## 6. 与 `ucf_projection_lab` 的数据格式对齐

实验室 `world_to_screen(p, m, w, h)`（`src/projection.cpp`）中 `m` 为 **16 元素列主序扁平数组**：
`m[col*4+row]`，对应 Unity `Matrix4x4` 字段顺序 `m00,m10,m20,m30,m01,...`。

- 真实使用需喂**视图投影矩阵** `VP = P * V`（列主序相乘）。
- Frida 端 `flattenCM()` 按“c 外、r 内”展平，即与实验室一致；
  宿主端用 `combine_pv(P, V)` 得到 `VP` 再喂入。
- 黄金样本（identity VP，1280×720）：
  - `world=(-0.35, 0.55, 0) -> screen=(416, 162)`
  - `world=(0.4, -0.45, 0) -> screen=(896, 522)`
  - `world=(0, 0, 0) -> screen=(640, 360)`
- 由 `tools/validate_projection.py` 离线复现验证（与 `src/projection.cpp` 公式逐字一致）。

列主序相乘（宿主端参考）：
```
VP[c*4+r] = Σ_k P[k*4+r] * V[c*4+k]    # (P*V)[row=r, col=c]
```

---

## 7. Frida 脚本映射（`tools/frida_dump.js`）

所有类名 / 方法名均来自上面 dump.cs 实证。**调用约定（踩过坑，务必照做）**：

```js
// 静态：Klass.method(名, 参数个数).invoke(实参...)
// 实例：obj.method(名, 参数个数).invoke(实参...)    ← this 由 obj 绑定，绝不作为参数传进去
```

| 目的 | 调用 |
|---|---|
| 主相机 | `Camera.method("get_main", 0).invoke()`（静态） |
| 视图 / 投影矩阵 | `cam.method("get_worldToCameraMatrix_Injected", 1).invoke(buf64)` / `get_projectionMatrix_Injected`，buf64 为 `Memory.alloc(64)` 的 out 缓冲 |
| 本地玩家 | `GameManager.field("myPlayer").value`（静态字段） |
| 单例实例 | **不要** `GameManager.method("get_instance", 0).invoke()`（泛型基类上的共享泛型方法，会 AV）。改读静态 `Singleton<T>.<instance>k__BackingField`，或 `Il2Cpp.gc.choose(GameManager)`，见第 3 节 |
| 全玩家 | `gm.field("allPlayers").value` → `arr.length` + **`arr.get(i)`**（没有 `arr[i]` 下标代理） |
| 坐标 | `p.method("get_transform", 0).invoke()` → `t.method("get_position_Injected", 1).invoke(buf12)` |
| 屏幕尺寸 | `Screen.method("get_width", 0).invoke()` / `get_height`（`UnityEngine.CoreModule`） |
| 队伍 / 是否自己 | 队伍**不要**用 `get_team()`（返回 boxed 对象，宿主无法比较，见下）；改裸读 `Entity.<team>k__BackingField` 的 int（`readTeamOf`）。是否自己用 `get_isMyPlayer()`（`Player`） |
| 生死 | **不要调** `get_isDead()`（有真实方法体，实测 AV）。由血量推导 `hp <= 0` |
| 血量 | `get_healthData()` → 裸内存读 `hiddenValue ^ currentCryptoKey`（见第 5 节） |
| 骨骼 | `get_characterAnimator()` → `GetBoneTransform(HumanBodyBones)` → `get_position_Injected(out Vector3)` |

> **反例（早期版本就是错在这里）：**
> `cam.method("get_worldToCameraMatrix").invoke(cam)` —— 多传了一个 `this`，运行期报
> `couldn't invoke method get_worldToCameraMatrix as it needs 0 parameter(s), not 1`。
> 所有实例方法都不要再传 `this`。

> **另四个坑：**
> `allPlayers[i]` 恒为 `undefined`（要用 `.get(i)`）；
> `get_instance()` 会让进程瞬间消失且不留异常栈（要改用静态 backing field / 堆扫描）；
> `get_isDead()` 同样会让进程消失（有真实方法体，且 frida 线程不在 Unity 主线程上）——
> **判读口诀：先看 dump 里有没有 `[CompilerGeneratedAttribute]`，没有就有方法体，要小心。**
> `get_team()` 虽然不会崩，但返回值是 **boxed 对象**：`Team` 是 `enum : int`
> （dump 231002：`value__`，0=BlackList 1=GlobalRisk 2=Neutral），而 bridge 把它按
> OBJECT 处理、不解包 `value__`，传到宿主是 `{"handle":..., "type":...}`。
> 两个对象永远不相等 → **队友识别整个失效，全员被当成敌人**。
> 所以改为裸读 `Entity.<team>k__BackingField`（0x1C）的 4 字节 int，零 invoke 且可比较。

契约校验：`python tests/test_dump_contract.py` 会解析 dump.cs，确认上表里的方法名/签名与
`Matrix4x4` 列主序、`ObscuredInt` 偏移全部成立（离线，不需要游戏）。

> 如方法名因 frida-il2cpp-bridge 版本不同而变化，把脚本里 `DISCOVER` 设为 `true`
> 即可打印关键类的真实导出名再对齐。
