# UCF Projection Lab

基于 Ghidra 静态初筛的 C++20 离线投影校准项目。默认使用代码内合成矩阵与坐标，不读取进程、不附加运行程序、不访问网络。

## 构建与验证

```sh
cmake -S . -B build
cmake --build build
(cd build && ctest --output-on-failure)
./build/ucf_projection_lab
```

## 输出

程序打印固定世界坐标的屏幕坐标；测试覆盖屏幕中心、左上角和相机后方三种情况。静态初筛报告见 `docs/STATIC_TRIAGE.md`。

## 运行时叠加层（本机 Windows，可选）

除离线投影实验室外，`tools/` 下还有一套针对**自有单机游戏 UnityCrossFire**（32 位 IL2CPP）的
运行时调试叠加层：清理同名残留进程 → 启动游戏 → 按 `INS` 注入 frida → 读相机矩阵/玩家坐标
→ 投影 → 透明叠加层绘制。

```sh
python tools/frida_host.py --no-overlay   # 只打印每帧标记
python tools/frida_host.py                # 带叠加层
python tools/frida_host.py --level 0      # 闪退排查：不碰 IL2CPP，只心跳（0~4 逐级二分）
python tools/frida_host.py --probe        # 单步探测：崩在哪一次调用，看最后一行 ▶[n/N]
python tools/frida_probe.py               # 只看进程：哪个 pid 才是真正跑 IL2CPP 的
python tests/test_process_selection.py    # 进程选择 + --game 路径解析，离线断言
python tests/test_dump_contract.py        # dump.cs <-> frida_dump.js 契约（偏移/方法名），离线
python tests/test_script_config.py        # 档位/探测/UCFG 是否源码与打包产物同步，离线
python tests/test_projection_calibration.py  # 真实对局帧锁死投影数学与 NDC 裁剪，离线
python tests/test_overlay_geometry.py     # 叠加层映射/DPI/失焦隐藏/防闪烁/鼠标穿透，离线
```

叠加层相关（位置偏移、画到别的窗口、闪烁、点击穿透）都有实测证据与修法记录，
见 `docs/HOST_OVERLAY.md` 的「框位置和实际对不上 / 画到别的窗口 / 闪烁」一节；
其中最关键的一条：宿主必须声明 **DPI 感知**（本机 250% 缩放下，
不声明时坐标系只有实际的一半多点，框会整体缩放错位）。

细节、参数与排错见 `docs/HOST_OVERLAY.md`；静态地址/偏移见 `docs/STATIC_REFERENCE.md`。

**边界**：仅用于授权范围内的单机游戏自测，不针对任何联机服务或第三方程序；离线部分默认不读进程、不联网。
