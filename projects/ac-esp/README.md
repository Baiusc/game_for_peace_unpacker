# ac-esp：Rust 覆盖层投影示例

`ac-esp` 是一个针对 **AssaultCube 1.3.0.2** 的 Rust 动态库学习样例。它演示了如何在同一进程内读取已知内存布局、将三维坐标通过视图矩阵投影到窗口平面，并把结果交给独立的 Windows 覆盖层线程绘制矩形。

> 本项目用于本地逆向工程与图形投影学习。源码中的窗口标题、模块名、偏移量和指针宽度均是版本绑定的示例数据；不要把它们当作通用接口或跨版本结论。

## 项目结构

| 路径 | 作用 |
| --- | --- |
| `src/lib.rs` | 动态库入口、窗口信息读取、采集循环和覆盖层线程协调。 |
| `src/model.rs` | 二维/三维坐标与实体字段读取的轻量数据模型。 |
| `src/offset.rs` | 目标版本绑定的模块/实体相对偏移量。 |
| `src/util.rs` | 内存读取封装及世界坐标到屏幕坐标的投影计算。 |
| `build.bat` | Windows MSVC 32 位目标的构建命令。 |
| `build_cross.sh` | GNU 32 位交叉构建命令。 |

## 运行流程解读

1. DLL 载入时，`DllMain` 在新线程启动 `run()`，避免在加载器回调内执行长循环。
2. `run()` 根据窗口标题取得客户区尺寸，创建与窗口对齐的覆盖层线程。
3. 程序读取实体列表地址、实体数量和 4×4 视图矩阵。
4. 每个有效实体的头部和脚部三维坐标会被投影为二维像素坐标。
5. 程序按头脚高度生成矩形，并通过 `Arc<RwLock<Vec<RECT>>>` 原子式地交换给绘制线程。

## 构建

### Windows / MSVC

```bat
build.bat
```

等价命令：

```sh
cargo build --release --target=i686-pc-windows-msvc
```

### GNU 交叉构建

```sh
./build_cross.sh
```

等价命令：

```sh
cross build --release --target=i686-pc-windows-gnu
```

构建输出位于对应 target 目录下，库文件名通常为 `ac_esp.dll`。

## 验证

在安装 Rust 工具链及目标平台后执行：

```sh
cargo check --target=i686-pc-windows-msvc
```

本次中文注释和文档改动不改变 Rust 逻辑；可通过下列命令检查文档和源码结构：

```sh
python3 documentation_evidence/verify_documentation.py
```

## 已知约束

- 模块名、偏移量及字段布局仅匹配标注的目标版本，升级或更换目标后必须重新验证。
- 代码使用 `u32` 保存地址，仅适用于 32 位地址模型。
- `read_memory` 中的解引用假设地址有效、对齐方式和数据布局正确；这些前提不成立时会导致未定义行为。
- 覆盖层窗口依赖目标窗口客户区，窗口大小或位置变化不会在当前实现中自动重建。

## 参考资料

- [About Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-windows)
- [Rust for Windows](https://kennykerr.ca/rust-getting-started)
- [Windows GDI](https://learn.microsoft.com/en-us/windows/win32/gdi/windows-gdi)
- [Windows Direct2D](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-portal)

## 许可证

[GNU Affero General Public License v3.0](https://choosealicense.com/licenses/agpl-3.0/)
