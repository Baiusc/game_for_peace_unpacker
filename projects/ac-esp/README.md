# AssaultCube-RE：C++ 离线实体投影与诊断示例

这是原 `ac-esp` 的 C++20 重构版本。项目使用**内置合成帧**演示实体过滤、三维坐标投影、矩形生成、配置加载、文本菜单和诊断输出；默认路径不读取游戏进程、不加载 DLL/驱动、不访问网络。

## 已完成的升级

- **C++20 / CMake**：移除 Cargo 与全部活动 Rust 源文件。
- **分层架构**：`EntitySource`、投影、配置、应用与测试分离。
- **默认菜单**：展示主题、窗口尺寸、数据源和诊断运行模式。
- **配置交互**：首次运行自动生成 `config.toml`；可修改窗口大小、主题、菜单与诊断开关。
- **帧快照**：读取接口返回不可变 `FrameSnapshot`，便于测试和后续替换数据后端。
- **验证**：包含投影、失效实体过滤和默认配置生成测试。

## 目录

| 路径 | 作用 |
| --- | --- |
| `include/ac_re/` | 公共模型、配置、数据源、投影和应用接口。 |
| `src/` | C++20 实现与默认入口。 |
| `tests/` | CTest 测试程序。 |
| `config.toml` | 首次运行自动生成的本地界面配置。 |
| `migration_evidence_cpp/` | Rust 到 C++ 迁移的原始快照、差异、验证和回滚脚本。 |

## 默认运行

无需参数：

```sh
cmake -S . -B build
cmake --build build
./build/assaultcube_re
```

预期输出包括菜单状态、两条合成实体矩形和诊断汇总。

## 配置示例

```toml
window_width = 1280
window_height = 720
max_entities = 32
show_menu = true
show_diagnostics = true
theme = cyan
```

## 验证

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/assaultcube_re
```

## 设计边界

- `FixtureEntitySource` 是默认且唯一启用的数据源，数据完全位于源码中。
- `EntitySource` 抽象用于测试、回放和离线数据导入；项目不实现进程附加、隐蔽加载或反检测逻辑。
- 投影函数以 4×4 矩阵为输入，适合用录制或合成帧验证边界条件。

## 许可证

[GNU Affero General Public License v3.0](https://choosealicense.com/licenses/agpl-3.0/)
