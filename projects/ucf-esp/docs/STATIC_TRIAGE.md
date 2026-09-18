<!--
 * @Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @Date         : 2026-09-18 09:39:05
 * @LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @LastEditTime : 2026-09-18 09:39:06
 * @FilePath     : /game_for_peace_unpacker/projects/ucf-esp/docs/STATIC_TRIAGE.md
 * @Description  : 
 * 
 * Copyright (c) 2026 by vitalchem, All Rights Reserved. 
-->
# UnityCrossFire.exe 静态初筛（Ghidra）

- 程序为 PE x86（32-bit），镜像基址 `0x00400000`。
- Ghidra 自动分析后识别 568 个函数、710 个符号。
- 导入项包含 `UnityMain`；字符串包含 `UnityPlayer.dll` 与 Unity 清单。
- `mono`、`il2cpp`、`GameAssembly`、`Assembly-CSharp`、`global-metadata.dat` 在主 EXE 字符串中无匹配。
- 因此 `UnityCrossFire.exe` 可确认为 Unity 启动入口；运行时图形/场景逻辑应与 `UnityPlayer.dll` 及同目录数据文件联合判断。


