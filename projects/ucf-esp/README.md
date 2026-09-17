# UCF Projection Lab

基于 Ghidra 静态初筛的 C++20 离线投影校准项目。默认使用代码内合成矩阵与坐标，不读取进程、不附加运行程序、不访问网络。

## 构建与验证

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/ucf_projection_lab
```

## 输出

程序打印固定世界坐标的屏幕坐标；测试覆盖屏幕中心、左上角和相机后方三种情况。静态初筛报告见 `docs/STATIC_TRIAGE.md`。
