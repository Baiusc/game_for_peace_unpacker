# ac-esp 子项目说明

## 目标

本目录是 Rust/Windows 覆盖层与三维投影的静态学习样例。阅读和修改时，应明确区分版本绑定的示例偏移与可复用的投影、线程同步概念。

## 文件职责

- `src/lib.rs`：DLL 入口、窗口查询、覆盖层线程和实体遍历。
- `src/model.rs`：实体、二维/三维坐标模型。
- `src/offset.rs`：目标版本相关的偏移常量。
- `src/util.rs`：投影计算和受限的泛型内存读取。

## 修改与验证

1. Rust 代码保持格式化，改动后优先执行 `cargo fmt --check` 与 `cargo check --target=i686-pc-windows-msvc`。
2. 在未安装 Rust 工具链的环境中，至少执行 `python3 documentation_evidence/verify_documentation.py`，并如实记录工具链缺失的结果。
3. Python 工具必须不带参数直接运行。
4. README 同步写清输入、输出、验证命令和版本绑定约束。
