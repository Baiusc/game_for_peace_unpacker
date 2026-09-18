# 踩坑记录 / 关键修改总表（ucf-esp）

> **给所有 agent + 人类开发者。** 新增踩坑**追加到第二节表格**，并在对应 deep-doc 的「排错」补细节。
> 原则：先查这张表再动手，不重复踩。
> 详细实证见：`HOST_OVERLAY.md`（Python 宿主）、`CPP_OVERLAY.md`（C++ 叠加层）、
> `STATIC_REFERENCE.md`（IL2CPP 偏移）、`STATIC_TRIAGE.md`（Ghidra）。

---

## 一、关键修改记录（按时间，新→旧）

| 日期 | 改动 | 原因 | 详见 |
| --- | --- | --- | --- |
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
