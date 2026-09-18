# 本地输入模拟模块

本模块只用于作者自研单机 UnityCrossFire 的关闭反作弊调试构建，运行在作者自己的机器上；
默认关闭，不联机、不针对第三方、不分发。

- `LocalInputSim::update()` 将按键上升沿作为一次 tap，后续按住帧只执行 trace 移动。
- tap 分支只有在准星进入容差后才产生一次按键按下/释放事件。
- hold 分支没有开火路径。
- Windows 后端使用公开 `SendInput`；Linux 核心构建使用空平台后端，单测通过 `Sender` mock 验证。
- 模块不读取或写入游戏内存、不 Hook、不注入游戏进程。
