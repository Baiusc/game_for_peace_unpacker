# game_for_peace_unpacker

个人学习实验室：围绕**自研单机游戏 UnityCrossFire**（Unity IL2CPP，32-bit）做的
IL2CPP 逆向、内存读取、投影计算、叠加层绘制练习。**仅用于自己拥有的单机游戏**，
不联机、不盈利、不分发，不针对任何在线多人游戏或第三方进程。

## 子项目

| 目录 | 主题 | 可运行内容 |
| --- | --- | --- |
| `projects/ucf-esp/` | **UnityCrossFire 调试工具（当前活跃）**：Python+tkinter 运行时叠加层 + C++ D3D11+ImGui 透明叠加层骨架、共享内存数据链路、通用 2D 可视化、ImGui 菜单、相机角度平滑 | `tools/frida_host.py`、`cpp_overlay/ucf_overlay_win32.exe` |
| `projects/ue_pak_archive_lab` | Unreal Engine PAK 文件格式与归档读写 | `UE_PAK_UNPACK`、`UE_PAK_REPACK` |
| `projects/network_auth_audit_lab` | 应用层协议兼容性与健壮性 | RC4/MD5 fixture、TCP 回环服务、字段变异测试 |
| `projects/kernel_memory_defense_lab` | Linux/Windows 内核行为防御原型 | 合成四级页表、模块隐藏信号关联、ABI 路由 |

> **ucf-esp 是主项目**：agent 协作入口见根 `AGENTS.md` 与 `projects/ucf-esp/AGENTS.md`；
> 踩坑与关键修改见 `projects/ucf-esp/docs/PITFALLS.md`。

## 快速开始（ucf-esp · 离线核心）

```bash
cd projects/ucf-esp/cpp_overlay
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure        # 可视化/平滑/目标选择/传输/配置（Linux 可跑）
```

核心库（投影契约、可视化、平滑、共享内存双缓冲、配置）**不依赖 D3D/Win32**，可在 Linux 编译测试；
Windows 渲染层被 `if(WIN32)` 隔离。Windows 完整叠加层 exe 由 GitHub Actions 编译并作为 artifact 发布，
直接拷到 Win11 虚拟机运行。详见 `projects/ucf-esp/cpp_overlay/README.md`。

## 其它子项目

```bash
cmake -S . -B build && cmake --build build         # ue_pak_archive_lab
python3 projects/kernel_memory_defense_lab/page_table_model.py
python3 projects/kernel_memory_defense_lab/abi_router.py
python3 projects/kernel_memory_defense_lab/module_visibility_audit.py; test $? -eq 1
python3 projects/network_auth_audit_lab/loopback_simulator.py
python3 projects/network_auth_audit_lab/mutation_test.py
```

各 Python 脚本均带默认参数可直接运行；默认路径不加载驱动、不访问真实物理内存、不附加进程、不连接外部服务。

## 工作区约定

- `data/`、`paks/`：本地授权测试数据，不纳入示例命令的默认输入。
- `doc/`：参考资料与历史笔记；PDF 仅作为防护和取证学习材料。
- `3rd_project/`：**只读参考**，公开逆向学习项目，仅借鉴通用技术点，不复制项目特定逻辑。
- `projects/`：可独立构建或运行的学习子项目。

边界与详细结构见根 `AGENTS.md` 与各子项目 `AGENTS.md`。
