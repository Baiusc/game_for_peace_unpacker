
# game_for_peace_unpacker

# 和平精英解包工具 

---

`pak_file`参数可在`launch.json`中的`args`中配置

## 一、子目录说明
- `data`   数据
- `doc`   文档
- `include`   头
- `src`   源文件

## 二、Git
推送`git push origin main`

清理 .gitignore 中已被跟踪的文件：
```bash
git rm -r --cached . // 
git add .
git commit 
```

## 三、CMake
### 1.构建
先清除缓存变量`[ -d build ] && rm -rf build/*`

在主目录用命令行 `cmake -B build -S .`  

或者指定构建类型为`Debug`：`cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug`

or指定构建类型为`Release`：`cmake -B build -S . -DCMAKE_BUILD_TYPE=Release`

### 2.编译
windows+vs:在主目录用命令行：`cmake --build build --config Debug -- /m:%NUMBER_OF_PROCESSORS%`

windows+vs:在主目录用命令行：`cmake --build build --config Release -- /m:%NUMBER_OF_PROCESSORS%`

linux+vscode:在主目录用命令行：`cmake --build build --config Debug -- -j$(nproc)`

linux+vscode:在主目录用命令行：`cmake --build build --config Release -- -j$(nproc)`

### 2.安装 （本算法项目未用到）
在主目录用命令行`cmake --install build --config Release`


## 四、vs中Release模式下调试代码

使用Release模式调试代码（Release模式下需要改动的几个设置选项）

> 配置都是选中：Release

1. 右键项目 -> C/C++ -> 常规 -> 调试信息格式 -> “程序数据库(/Zi)”
2. 右键项目 -> C/C++ -> 优化 -> 优化 -> “已禁用(/Od)”
3. 右键项目 -> 链接器 -> 调试 -> 生成调试信息 -> “生成调试信息(/DEBUG)”

> 解决，Release无法调试问题

CMAKE语句：
```bash
# Release模式下调试代码
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # 配置 C/C++ 的调试信息格式为 “程序数据库(/Zi)”
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    
    # 配置 C/C++ 的优化级别为 “已禁用(/Od)”
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od")
    
    # 配置链接器生成调试信息 “生成调试信息(/DEBUG)”
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif()
```

## 五、将DLL复制到运行时输出目录

CMAKE语句：
```bash
# 将DLL复制到运行时输出目录
if (WIN32)
  add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS
  )
endif ()
```