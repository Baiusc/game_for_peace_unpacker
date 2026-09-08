# reverse-learning-lab 工作说明

## 项目概要

这是一个通用、离线的逆向学习项目。核心内容包括归档格式解析、网络协议安全审计与 Android 加载器静态取证。所有练习以仓库内合成样本或已获授权的本地文件为输入。

## 当前结构

- `projects/ue_pak_archive_lab`：现有 UE PAK 解包/打包功能。
- `projects/network_auth_audit_lab`：客户端认证协议的风险识别与整改清单。
- `projects/android_loader_forensics_lab`：Android 自解压包装和嵌入载荷的静态分析。
- `doc/`：参考 PDF 与历史资料。

## 开发约定

1. 始终用中文交流。
2. Python 脚本必须在不传入命令行参数时可直接运行。
3. 新增分析工具默认只处理仓库内合成样本；不得执行、加载或部署被分析载荷。
4. 修改 C/CMake 后运行 `cmake -S . -B build && cmake --build build`；修改 Python 后至少运行默认命令。
5. README 需要写明输入、输出、验证命令和学习边界。
