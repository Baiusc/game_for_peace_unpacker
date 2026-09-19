# 3rd 可见性实现对照与 UCF 修复记录

## 1. 参考过程

本轮只读检查了以下实现，未复制项目专属偏移、地址或反作弊逻辑：

| 项目 | 文件 | 可见性判定 |
|---|---|---|
| CS2_Lumen | `3rd_project/CS2_Lumen_External/Features/Aimbot.h` | 读取实体 spotted 位；在选靶过滤阶段直接跳过不可见目标 |
| Khytt | `3rd_project/Khytt External v3.0/src/sdk/game.h`、`src/features/aimbot.cpp` | 读取 spotted mask；不可见目标不进入候选集 |
| Valorant | `3rd_project/Valorant_External/Game/cheat.hpp` | 比较 `last_submit_time` 与 `last_render_time`，以约 0.06 秒阈值判断 |
| DeltaForce | `3rd_project/DeltaForce_External/DeltaForce_External/Game/Engine.cpp` | `IsVisable(MeshPtr)` 为真时才参与选靶，并按状态绘制颜色 |

四个项目的共同结构是：

```text
游戏/引擎维护的可见性状态 -> 统一 bool 谓词 -> 选靶过滤 -> 状态着色
```

它们并不是“对玩家根节点做一条物理射线”。

## 2. UCF 原实现为何会多数黄色

UCF 的 `Frame/PlayerState` 没有原生 `visible` 字段，也没有 dump 中已验证的
`lastSubmitTime/lastRenderTime` 或 spotted mask。因此不能直接照搬 3rd 的字段。

原实现只能使用：

```text
Camera.transform.position
  -> Physics.Linecast(camera, player.get_transform().position)
  -> RaycastHit.m_Distance 与总距离比较
```

问题在终点选择，而不只是 `m_Distance` 偏移：玩家根 Transform 通常在脚底或角色容器
根部，Linecast 可能先撞到目标自己的胶囊体/触发器。此前只给根节点 1.5m 末端容差，
仍不足以覆盖不同模型的根节点偏移；单条射线也无法表达“脚被矮墙挡住但头仍可见”。

## 3. 本轮采用的通用修复

参考 3rd 的“按瞄准骨位判断可见”思想，但不复制游戏专属字段：

1. 保留 root 作为无骨骼回退点。
2. 额外尝试现有 `Animator.GetBoneTransform` 的 Head、Chest、Hips。
3. 每个点独立执行同一条 Unity `Physics.Linecast`。
4. 任一取点没有命中障碍，或命中位置只落在目标自身末端容差内，则返回 `visible=true`。
5. 所有有效取点都在目标之前命中，才返回 `visible=false`。
6. 所有命中距离无效时 fail-open，避免 32-bit bridge 的 `RaycastHit` 回写异常把全场变黄。

参数：

```text
root: 1.5m       // 根节点可能位于脚底
bone: 0.35m      // 骨位在身体内部，使用更小的末端容差
```

## 4. 为什么没有直接采用 render-time/spotted

这些字段属于各自游戏的实体或 Mesh 数据。当前 UCF `dump.cs` 与
`docs/STATIC_REFERENCE.md` 没有已验证的等价字段；凭名称猜偏移会把未知值伪装成
可见性，风险高于通用物理取点。因此本轮只改 `frida_dump.js` 的 Unity API 路径，不改
Frame 契约，不新增未验证偏移。

## 5. Unity 原生 Renderer.isVisible 落地

后续静态复核确认 UCF 自己的 Unity dump 已包含：

```text
Renderer.isVisible / Renderer.get_isVisible()  RVA 0x43C170
SkinnedMeshRenderer : Renderer
Component.GetComponentInChildren(Type, bool)
Player.characterContainer @ 0x4C
```

代码现在优先从 `characterContainer` 子树取得 `SkinnedMeshRenderer`，调用
`get_isVisible()`；取不到 Renderer 或 bridge 调用失败时，才回退到 root + Head/Chest/Hips
多取点 Linecast。这样不再把 Linecast 作为正常路径的唯一依据。

注意：`Renderer.isVisible` 的语义是“被任意 Camera 认为可见”，不是绝对的主相机无遮挡。
如果游戏存在小地图或其它 Camera，后续应继续验证 Renderer 来源；当前角色主模型路径
优先使用 SkinnedMeshRenderer，避免把武器特效当作主体可见性。

## 6. 实机诊断

刷新周期仍由 `--vis-refresh-ms` 控制。首次刷新会打印：

```text
[*] 可见性样本 source=Renderer.isVisible visible= true
```

只有 Renderer 不可用并进入 Linecast 回退时，才会看到：

```text
visible=false（所有取点均被提前命中）
```

此时应检查 Unity 物理层是否把角色/触发器算作障碍，而不是继续无限增大容差。
当前 Linecast 使用 `-5` layer mask 和 `QueryTriggerInteraction.UseGlobal`；若确认触发器
造成误判，应在自研游戏物理层配置中排除触发器或提供已验证的障碍层。
