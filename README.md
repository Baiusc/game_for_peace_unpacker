# reverse-learning-lab

面向**离线、授权样本**的通用逆向学习工作区。项目将文件格式分析、网络协议审计和 Android 样本取证拆分为独立子项目，便于在可复现的本地环境中学习二进制结构、完整性校验和防护设计。

## 子项目

| 目录 | 主题 | 可运行内容 |
| --- | --- | --- |
| `projects/ue_pak_archive_lab` | Unreal Engine PAK 文件格式与归档读写 | `UE_PAK_UNPACK`、`UE_PAK_REPACK` |
| `projects/network_auth_audit_lab` | 客户端网络验证协议的离线审计 | 合成请求轨迹审计脚本 |
| `projects/android_loader_forensics_lab` | Android 自解压加载器的静态取证 | 合成 hex-ELF 载荷提取脚本 |

## 快速开始

```bash
cmake -S . -B build
cmake --build build
python3 projects/network_auth_audit_lab/analyze_fixture.py
python3 projects/android_loader_forensics_lab/extract_hex_elf.py
```

各 Python 脚本均带默认参数，可直接运行。两套审计实践只读取仓库内的合成离线样本；不发起网络请求、不加载模块、不附加进程，也不修改第三方软件或数据。

## 工作区约定

- `data/`、`paks/`：本地授权测试数据，不纳入示例命令的默认输入。
- `doc/`：参考资料与历史笔记；PDF 仅作为防护和取证学习材料。
- `projects/`：可独立构建或运行的学习子项目。

详细的边界、输入格式和验证方法见各子项目 README。
