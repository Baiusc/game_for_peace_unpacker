# UnityCrossFire.exe 静态初筛（Ghidra）

- 程序为 PE x86（32-bit），镜像基址 `0x00400000`。
- Ghidra 自动分析后识别 568 个函数、710 个符号。
- 导入项包含 `UnityMain`；字符串包含 `UnityPlayer.dll` 与 Unity 清单。
- `mono`、`il2cpp`、`GameAssembly`、`Assembly-CSharp`、`global-metadata.dat` 在主 EXE 字符串中无匹配。
- 因此 `UnityCrossFire.exe` 可确认为 Unity 启动入口；运行时图形/场景逻辑应与 `UnityPlayer.dll` 及同目录数据文件联合判断。

本目录的 C++ 代码只实现合成矩阵和坐标的离线投影校准，用于验证矩阵行列序、NDC 和窗口坐标映射。
