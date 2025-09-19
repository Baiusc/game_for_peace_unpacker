<!--
 * @Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @Date         : 2025-09-02 16:42:02
 * @LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @LastEditTime : 2025-09-19 13:33:36
 * @FilePath     : /game_for_peace_unpacker/README.md
 * @Description  : 
 * 
 * Copyright (c) 2025 by vitalchem, All Rights Reserved. 
-->

# game_for_peace_unpacker

# 和平精英解包/打包/paks/obb改文件 

---

打包目前存在问题：打包大obb文件会出现未替换而是添加的情况，导致大小膨胀

明文解包：

src/game_for_peace_unpack.c

`pak_file`参数可在`launch.json`中的`args`中配置

解包/打包 bms 江川工具 修改后可在PC端运行：

python/bms.py

grep搜索：

搜索当前目录下所有 .dat 文件中是否包含 "mk14"	`grep -l "mk14" *.dat`

递归搜索某个目录下的 .dat 文件	`grep -rl "mk14" /path/to/dir --include="*.dat"`

忽略大小写搜索	`grep -ril "mk14" *.dat`

如果你有 .dat 文件是二进制文件，grep 可能默认跳过。你可以加 -a 参数把它当作文本处理：

`grep -la "mk14" *.dat`

当前使用的grep： BP_ShootWeaponProjectileBase_C

`find . -name "*.dat" -print0 | xargs -0 grep -ail "BP_ShootWeaponBase_C"`

find . -name "*.dat" -print0 | xargs -0 grep -ail "BP_ShootWeaponProjectileBase_C" | xargs grep -ail "GameDeviationAccuracy" | xargs grep -ail "CameraShakeTemplate_AimCameraMode"


命令解析：

find . -name "*.dat" -print0:

find .：在当前目录 (.) 下开始查找文件。

-name "*.dat"：只查找所有以 .dat 结尾的文件。

-print0: 将找到的文件名以空字符 (\0) 分隔，而不是换行符。这能正确处理文件名中包含空格的情况。

|: 管道符，将 find 命令的输出作为 xargs 命令的输入。

xargs -0 grep -ail "BP_ShootWeaponBase_C":

xargs -0: 接收 find 传来的以空字符分隔的文件名列表。

grep -ail "BP_ShootWeaponBase_C": 对每个文件名执行 grep 命令，使用 -a (当作文本)、-i (忽略大小写) 和 -l (只列出文件名) 标志进行搜索。

这个方法更加健壮，因为它强制 grep 逐个处理文件，并且 xargs 的 -0 选项确保了文件名能够被正确传递，有效地避免了之前遇到的“可执行文件格式错误”问题。

## 零、dat对应关系

打印当前文件夹下 map_lobby_1.33.12.14210.pak 文件的实际大小： ls -l map_lobby_1.33.12.14210.pak

打印当前文件夹下 game_patch_1.32.11.14059.pak 文件的实际大小：ls -l game_patch_1.32.11.14059.pak

ls -l map_lobby_1.32.11.13800_0.67.pak


map_lobby_1.33.12.14210.pak 794.6 MB (794598863 字节)
- `./file_0/00000009.dat`   BP_ShootWeaponBase_C
- `./file_0/00000051.dat`   BP_ShootWeaponBase
- `./file_0/00000013.dat`   BP_ShootWeaponProjectileBase_C
- `./file_0/00000057.dat`   BP_ShootWeaponProjectileBase_C
- `./file_1000/00001116.dat`   BP_ShootWeaponProjectileBase_C

## 键值对功能说明书

SkeletalBodySetup 范围 find . -name "*.dat" -print0 | xargs -0 grep -ail "SkeletalBodySetup" 

20250919最新：打印文件名+文件大小字节 find . -name "*.dat" -type f -exec grep -ail "SkeletalBodySetup" {} + | xargs -I {} stat --format="%s %n" {}

搜索文件大小为25651字节的.dat文件并打印文件名和大小：find . -name "*.dat" -type f -size 25651c -exec stat --format="%s %n" {} \;

find . -name "*.uasset" -type f -exec grep -ail "SkeletalBodySetup" {} + | xargs -I {} stat --format="%s %n" {}

./Content/Arts_Player/Characters/Animation/Base_Skeleton/CH_Base_SK_PhysicsAsset.uasset

5893字节 ./file_6/00000365.dat

17035字节

CH_Base_SK_PhysicsAsset 骨骼box大小 范围  SkeletalBodySetup

部位名称及其含义
pelvis：骨盆。这是身体的中心，连接上半身和腿部。

head：头部。角色的头，通常是游戏中最重要的命中区域，常用于爆头判定。

thigh_r：右大腿。thigh 是大腿，_r 表示右侧（right）。

calf_r：右小腿。calf 是小腿，_r 表示右侧。

lowerarm_l：左前臂。lowerarm 是前臂，_l 表示左侧（left）。

foot_l：左脚。foot 是脚，_l 表示左侧。

spine_03：脊柱。spine 是脊柱，_03 通常表示这是脊柱的第三节或特定部位，用于区分不同的脊椎骨。

upperarm_l：左上臂。upperarm 是上臂，_l 表示左侧。

AccessoriesVRecoilFactor（配件垂直后坐力系数）0.55 表示配件将垂直后坐力降低至原始值的 55%  改为 0.01418

AccessoriesHRecoilFactor（配件水平后坐力系数）0.9 表示配件将水平后坐力降低至原始值的 90%   改为 0.02418

AccessoriesRecoveryFactor（配件后坐力恢复系数）0.65 表示配件将恢复速度提升至原始值的 65% （即加快恢复）。
数值越大，准星在射击后更快回正（适合快速连续点射）。改为 0.9876

GameDeviationFactor（全局偏差系数）数值越小，子弹落点越集中。从 3.024 改为 0.1418 据点

CrossHairBurstSpeed（准星爆发速度）数值越低，准星扩散越平缓。从 18.0 改为 0.2418 跳弹

CrossHairBurstIncreaseSpeed（准星爆发增速）数值越低，扩散速度变化更线性（后坐力表现更平滑）。从 3.5 改为 0.1418

RecoilKickADS 代表 "Aim Down Sights Recoil Kick"，即开镜瞄准（ADS）时的后坐力冲击。从 0.15 改为 0.025123

## 打包

你提供的代码中的 repack_pak 函数实际上只是简单地复制了旧文件的数据和索引，并没有真正进行“重新打包”，这对于修改文件后进行打包是无效的。我将根据你提供的文件结构（PakInfo, Entry 等），重新设计并实现一个完整的 repack_pak 函数。

这个新函数将遵循以下逻辑：

首先，它会像你最初提到的那样，先写入一个占位符头部。

然后，它会逐个读取你想要打包的源文件，将它们的内容写入到新的PAK文件中，并记录每个文件在新PAK中的偏移量和大小。

接下来，它会根据这些记录的偏移量和大小，在内存中构建一个新的索引。

然后，将这个新构建的索引写入到文件的末尾。

最后，它会计算出正确的索引位置和大小，更新并写入最终的 PakInfo 头部。



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