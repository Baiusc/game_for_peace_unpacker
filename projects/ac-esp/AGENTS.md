# AssaultCube-RE 工作说明

## 项目目标

本目录是 C++20/CMake 的离线实体投影与诊断样例。默认执行只使用内置合成帧，用于验证配置、菜单状态、实体过滤和投影算法。

## 开发约定

1. 始终使用中文文档与注释。
2. C++ 改动后执行 `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`。
3. 默认程序不带参数运行，并自动创建缺失的 `config.toml`。
4. 新数据源实现必须可离线测试，并保持 `FixtureEntitySource` 为默认数据源。
5. README 同步说明输入、输出、验证命令和设计边界。
