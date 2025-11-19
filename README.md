<!--
 * @Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @Date         : 2025-09-02 16:42:02
 * @LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @LastEditTime : 2025-11-13 17:31:56
 * @FilePath     : /game_for_peace_unpacker/README.md
 * @Description  : 
 * 
 * Copyright (c) 2025 by vitalchem, All Rights Reserved. 
-->

# game_for_peace_unpacker

# 和平精英解包/打包/paks/obb改文件 

---

# Android Stutio 绝对路径启动模拟器 手动启动模拟器并启用可写系统

/home/baiusc/Android/Sdk/emulator/emulator -avd Medium_Phone_API_36.1 -writable-system -no-snapshot

## 一、子目录说明
- `data`   数据
- `doc`   文档
- `include`   头
- `src`   源文件

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

bEnableExtraSphereCollision ExtraSphereCollisioConfig 	启用额外的球形碰撞体。它意味着投射物除了本身的碰撞体外，还会使用一个额外的、通常更大的球形碰撞体来进行命中检测。

RadiusScaleCurve 半径缩放曲线引用。 这是最重要的配置之一。它引用了一个曲线资产 (ObjectPropertyData)，意味着这个额外球形碰撞体的半径不是固定的，而是随着时间、速度或距离等因素动态变化的。这在游戏中常用于： 1. 增加远距离命中容错率（"子弹磁铁"）：在远距离，投射物的碰撞体积增大，使玩家更容易命中目标。 2. 优化高速移动检测： 确保在极高速下，即使主碰撞体穿透了，这个更大的扩展碰撞体也能正确触发命中。


find . -type f -name '*BP_Player*Bullet*'
find . -type f -name '*BP_*_Bullet*'
find . -type f -name '*Bullet*Template*'
示例 (搜索 "Weapon" 或 "Damage" 或 "Mesh" 并在上下各打印 3 行):

find . -type f -name "*.uasset" -exec grep -H -E 'BP_PlayerBoltBullet|BP_PlayerSPAS12ShotgunBullet' -C 3 {} +
find . -type f -name "*.uasset" -exec grep -H -l -E 'BulletTemplate' {} +
find . -type f -name "*.uasset" -exec grep -H -l -E 'CH_Base_SK_PhysicsAsset' {} +

{"$type":"UAssetAPI.PropertyTypes.Objects.ObjectPropertyData, UAssetAPI","Name":"BulletTemplate","ArrayIndex":0,"IsZero":false,"PropertyTagFlags":"None","PropertyTagExtensions":"NoExtension","Value":-20}

SphereRadius 子弹球体半径 BP_PlayerRifleBullet.uasset BP_PlayerSniperBullet_ BP_PlayerDMRBullet_

SkeletalBodySetup 范围 find . -name "*.dat" -print0 | xargs -0 grep -ail "SkeletalBodySetup" 

20250919最新：打印文件名+文件大小字节 find . -name "*.dat" -type f -exec grep -al "SkeletalBodySetup" {} + | xargs -I {} stat --format="%s %n" {}

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

(ros:noetic)baiusc@bzs-work-pc:/media/baiusc/DATA1/Repositories/ReposMyself/pubg-unpacker/game_for_peace_unpacker/paks$ find . -type f -name "*.uasset" -exec grep -H -l -E 'BP_PlayerBoltBullet' {} +
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CrossBow/BP_Other_CrossBow.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerBoltBullet_Big.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CrossbowBorderland/BP_CrossbowBordBullet_Big.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerBoltBullet.uasset

(ros:noetic)baiusc@bzs-work-pc:/media/baiusc/DATA1/Repositories/ReposMyself/pubg-unpacker/game_for_peace_unpacker/paks$ find . -type f -name "*.uasset" -exec grep -H -l -E 'BP_PlayerBoltBullet_Big' {} +
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CrossBow/BP_Other_CrossBow.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerBoltBullet_Big.uasset

(ros:noetic)baiusc@bzs-work-pc:/media/baiusc/DATA1/Repositories/ReposMyself/pubg-unpacker/game_for_peace_unpacker/paks$ find . -type f -name "*.uasset" -exec grep -H -l -E 'BulletTemplate' {} +
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponBase.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CrossBow/BP_Other_CrossBow.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/Flaregun/BP_Pistol_Flaregun.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/SawedOff/BP_ShotGun_SawedOff.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Rifle/M16A4/BP_Rifle_M16A4.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Rifle/Mk47/BP_Rifle_Mk47.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/ShotGun/BP_ShotGunBase.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/AWM/BP_Sniper_AWM.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Kar98K/BP_Sniper_Kar98k.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/M24/BP_Sniper_M24.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Mini14/BP_Sniper_Mini14.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Mk14EBR/BP_Sniper_Mk14.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/QBU/BP_Sniper_QBU.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/SKS/BP_Sniper_SKS.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/SLR/BP_Sniper_SLR.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/VSS/BP_Sniper_VSS.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Win94/BP_Sniper_Win94.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponNewBase.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponProjectileBase.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CompoundBow/BP_Other_CompoundBow.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CrossbowBorderland/BP_Other_CrossbowBorderland.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/HuntingBow/BP_Other_HuntingBow.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/HuntingBow/BP_ShootWeaponBowBase.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/M79SmokeGrenadeLauncher/BP_Other_M79SmokeGrenadeLauncher.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/MG3/BP_Other_MG3.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/Mortar/BP_Other_Mortar.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/StunGun/BP_Other_StunGun.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/ShotGun/SPAS-12/BP_ShotGun_SPAS-12.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/AMR/BP_Sniper_AMR.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/M200/BP_Sniper_M200.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/M417/BP_Rifle_M417.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/MK12/BP_Sniper_MK12.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/MK20/BP_Sniper_MK20.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Mosin/BP_Sniper_Mosin.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/SVD/BP_Sniper_SVD.uasset
./ShadowTrackerExtra_patch_14320原厂/Content/Actor_Timeliness/CG026_Resurrection/Blueprints/RevivalFlareGun/BP_Pistol_RevivalFlaregun.uasset
./ShadowTrackerExtra_patch_14320原厂/Content/UGC/UGCGame/Weapon/MainWeapon/Pistol/Flaregun/BP_UGC_Pistol_Flaregun.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Battlefield/AT4-A_BF/BP_Other_AT4-A_BF.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponProjectileBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/AT4-A/BP_Other_AT4-A.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CompoundBow/BP_Other_CompoundBow.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/HuntingBow/BP_Other_HuntingBow.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/HuntingBow/BP_ShootWeaponBowBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/M3E1/BP_Other_M3E1.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/MG3/BP_Other_MG3.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/PanzerFaust/BP_Other_PanzerFaust.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/FireworkGun/BP_Pistol_FireworkGun.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/Flaregun/BP_Pistol_Flaregun.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/AMR/BP_Sniper_AMR.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/AWM/BP_Sniper_AWM.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Kar98K/BP_Sniper_Kar98k.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/M200/BP_Sniper_M200.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/M24/BP_Sniper_M24.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Mini14/BP_Sniper_Mini14.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Mk14EBR/BP_Sniper_Mk14.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/Mosin/BP_Sniper_Mosin.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/SKS/BP_Sniper_SKS.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Sniper/SVD/BP_Sniper_SVD.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_Timeliness/GameMode/SuperPeople/BluePrints/CowBoy/Weapon/BP_Cowboy_Win94.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Mod/BattleField/BluePrints/PlayerCareer/Engineer/SkillActor/BP_BattleFiledTurretWeapon.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Mod/BattleField/BluePrints/Weapon/BattleField_Fort/BP_Other_Fort.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Mod/BattleField/BluePrints/Weapon/MedicalGun_BF/BP_Pistol_MedicalGun_BF.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Mod/Escape/Art_Player/Vehicle/PatrolCar/BP_Escape_PatrolCar_Gatlin.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Mod/Escape/BluePrints/Weapon/MainWeapon/Other/RDCrossBow/BP_CG030_HandheldBow_Escape.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/UGC/UGCGame/Weapon/MainWeapon/BP_UGC_ShootWeaponBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/UGC/UGCGame/Weapon/MainWeapon/BP_UGC_ShootWeaponBowBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/UGC/UGCGame/Weapon/MainWeapon/BP_UGC_ShootWeaponNewBase.uasset

(ros:noetic)baiusc@bzs-work-pc:/media/baiusc/DATA1/Repositories/ReposMyself/pubg-unpacker/game_for_peace_unpacker/paks$ find . -type f -name "*.uasset" -exec grep -H -l -E 'BP_PlayerRifleBullet' {} +
./ShadowTrackerExtra_map_filter原厂/Content/Arts_PlayerBluePrints/Vehicle/ArmedVehicle/VehicleWeapons/VehGatlin.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponBase.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/DesertEagle/BP_Pistol_DesertEagle.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/Flaregun/BP_Pistol_Flaregun.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/P18C/BP_Pistol_P18C.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/P1911/BP_Pistol_P1911.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/R1895/BP_Pistol_R1895.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/R45/BP_Pistol_R45.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/SawedOff/BP_ShotGun_SawedOff.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/Vz61/BP_Pistol_Vz61.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBullet.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBulletDamageType.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSniperBulletDamageType.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponNewBase.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponProjectileBase.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CrossbowBorderland/BP_CrossbowBordDamageType.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/CrossbowBorderland/BP_CrossbowBordDamageType_Burn.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/HuntingBow/BP_ShootWeaponBowBase.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/M79SmokeGrenadeLauncher/BP_Other_M79SmokeGrenadeLauncher_Bullet.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/M79SmokeGrenadeLauncher/BP_Other_M79SmokeGrenadeLauncher_GrenadeBullet.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/Mortar/BP_Other_Mortar_Bullet.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/Mortar/BP_Other_Mortar_Bullet_Burn.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/Mortar/BP_Other_Mortar_Bullet_Poison.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/TMP/BP_Pistol_TMP.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerExplosionArrowDamageType.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBulletImpact.uasset
./ShadowTrackerExtra_patch_14320原厂/Content/Actor_Timeliness/CG026_Resurrection/Blueprints/RevivalFlareGun/BP_Pistol_RevivalFlaregun.uasset
./ShadowTrackerExtra_patch_14320原厂/Content/UGC/UGCGame/Weapon/MainWeapon/Pistol/ColtAnaconda/BP_UGC_Pistol_ColtAnaconda.uasset
./ShadowTrackerExtra_patch_14320原厂/Content/UGC/UGCGame/Weapon/MainWeapon/Pistol/Flaregun/BP_UGC_Pistol_Flaregun.uasset
./ShadowTrackerExtra_patch_14320原厂/Content/UGC/UGCGame/Weapon/MainWeapon/Pistol/Vz61/BP_UGC_Pistol_Vz61.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Battlefield/AT4-A_BF/BP_Other_AT4-A_BF.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/BP_ShootWeaponProjectileBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/AT4-A/BP_Other_AT4-A.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/HuntingBow/BP_ShootWeaponBowBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/M3E1/BP_Other_M3E1.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/FireworkGun/BP_Pistol_FireworkGun.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/Flaregun/BP_Pistol_Flaregun.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/P18C/BP_Pistol_P18C.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/P1911/BP_Pistol_P1911.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Pistol/R1895/BP_Pistol_R1895.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Mod/BattleField/BluePrints/Weapon/MedicalGun_BF/BP_Pistol_MedicalGun_BF.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/Mod/Escape/BluePrints/Weapon/MainWeapon/Pistol/Crystal/BP_Pistol_Crystal.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/UGC/UGCGame/Weapon/MainWeapon/BP_UGC_ShootWeaponBase.uasset
./ShadowTrackerExtra_patch_14323原厂/Content/UGC/UGCGame/Weapon/MainWeapon/BP_UGC_ShootWeaponBowBase.uasset

(ros:noetic)baiusc@bzs-work-pc:/media/baiusc/DATA1/Repositories/ReposMyself/pubg-unpacker/game_for_peace_unpacker/paks$ find . -type f -name '*BP_Player*Bullet*'
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_Scenes/Festival/CG05_WinnerAirdrop/BP_PlayerFlareGunBullet_Winner.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/Arts_Scenes/Festival/CG05_WinnerAirdrop/BP_PlayerFlareGunBullet_Winner.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/Pistol/BP_PlayerPistolBulletDamageType.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/Pistol/BP_PlayerPistolBulletDamageType.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerM16A4Bullet.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerBoltBullet_Big.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerBoltBullet_Big.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerDMRBullet_Big.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerDMRBullet_Big.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerFlareGunBullet.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerFlareGunBullet.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerM16A4Bullet.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBullet.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBullet.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBulletDamageType.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBulletDamageType.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerShotgunBullet.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerShotgunBullet.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSniperBullet.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSniperBullet.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSniperBulletDamageType.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSniperBulletDamageType.uexp
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSniperBullet_Big.uasset
./ShadowTrackerExtra_map_lobby原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSniperBullet_Big.uexp
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerAMRBulletDamageType.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerAMRBulletDamageType.uexp
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerBoltBullet.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerBoltBullet.uexp
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerCompoundBowBullet_Big.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerCompoundBowBullet_Big.uexp
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerHurtBoltBullet_Big.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerHurtBoltBullet_Big.uexp
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBulletImpact.uasset
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBulletImpact.uexp
./ShadowTrackerExtra_map_weapon原厂/Content/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerSPAS12ShotgunBullet.uasset
(ros:noetic)baiusc@bzs-work-pc:/media/baiusc/DATA1/Repositories/ReposMyself/pubg-unpacker/game_for_peace_unpacker/paks$ 

BP_PlayerM16A4Bullet_C
Default__BP_PlayerM16A4Bullet_C
意外自动改变的键值对Name: PlugComponentSlotMap   BreakThroughDampRateConfig
/Game/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerM16A4Bullet
/Game/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBullet

BP_PlayerBoltBullet.uasset 战术弩
BP_PlayerBoltBullet.uexp

BP_PlayerCompoundBowBullet_Big.uasset 复合弓的投射物
BP_PlayerCompoundBowBullet_Big.uexp

BP_PlayerHurtBoltBullet_Big.uasset 爆炸猎弓的投射物 
BP_PlayerHurtBoltBullet_Big.uexp

BP_PlayerSPAS12ShotgunBullet.uasset  SPAS-12 霰弹的投射物
BP_PlayerSPAS12ShotgunBullet.uexp

/Game/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_Other_GM3_Bullet

BP_Other_GM3_Bullet_C

Default__BP_Other_GM3_Bullet_C

/Game/BluePrints/Weapon/PlayerWeapon/RifleGun/BP_PlayerRifleBullet

BP_PlayerRifleBullet_C

Default__BP_PlayerRifleBullet_C

20251029 准备明天来检查 285.dat 是不是从14362补丁pak解压出来的，版本是否对得上

20251104 天线改为修改 283.dat 双向替换后，打包大小正常。也就是 BP_STRUCT_AvatarBPTable_type







  // 读取目录和文件映射表

    uint64_t ENTRIES = 0; // 未知用途

    uint64_t DIR_COUNT = 0; // 目录数量

    

    read_data(&ENTRIES, IndexData, 8);

    read_data(&DIR_COUNT, IndexData, 8);

    

    int32_t DIR_LEN = 0;

    char DIR_NAME[1024];

    uint64_t DIR_FILES = 0;

    int32_t ENTRY; // 索引，指向之前读取的Entry数组

    

    char path[1024];

    

    // 遍历所有目录

    for (int files = 0; files < DIR_COUNT; files++) {

        read_data(&DIR_LEN, IndexData, 4);

        read_data(DIR_NAME, IndexData, DIR_LEN);

        read_data(&DIR_FILES, IndexData, 8);

        

        // 遍历当前目录下的所有文件

        for (int x = 0; x < DIR_FILES; x++) {

            read_data(&FilenameSize, IndexData, 4);

            

            if (FilenameSize > 0) {

                read_data(Filename, IndexData, FilenameSize); // 如果文件名是 ASCII 编码 

            } else {

                // 如果文件名是 unicode 编码

                read_data(Filename, IndexData, -FilenameSize * 2);

                if (unicode_to_utf8(Filename, -FilenameSize * 2, Filename, sizeof(Filename)) == -1) {

                    printf("failed to convert UTF-16LE filename into UTF-8!\n");

                    exit(1);

                }

            }

            

            read_data(&ENTRY, IndexData, 4);

            

            // 构建完整的文件路径 

            memset(path, 0, 1024);

            snprintf(path, 1024, "%s%s%s", MountPoint, DIR_NAME, Filename);

            

            // 若目标特征符合，则保存目标为本地txt文件

            if ( strcmp(Filename, "BP_UGC_ShotGun_S12K.uasset") == 0 || strcmp(Filename, "BP_UGC_ShotGun_S12K.uexp") == 0 || strcmp(Filename, "BP_PlayerRifleBullet.uasset") == 0)

            {

                // 构建输出文件名：[Filename].txt

                char outputFilename[1024];

                snprintf(outputFilename, 1024, "%s_info.txt", Filename);

                FILE *logFile = fopen(outputFilename, "w");

                if (logFile == NULL) {

                    fprintf(stderr, "Error opening log file: %s\n", outputFilename);

                } else {

                    fprintf(logFile, "找到目标: %s\n", path);



                    // 打印 DIR_LEN DIR_NAME DIR_FILES FilenameSize Filename ENTRY 等原始索引键值对

                    fprintf(logFile, "\n--- 原始索引键值对 ---\n");

                    fprintf(logFile, "ENTRY Index: %d\n", ENTRY); // ENTRY 是索引

                    fprintf(logFile, "DIR_LEN: %d\n", DIR_LEN);

                    fprintf(logFile, "DIR_NAME: %s\n", DIR_NAME);

                    fprintf(logFile, "DIR_FILES (Files in Dir): %llu\n", DIR_FILES);

                    fprintf(logFile, "FilenameSize (Raw): %d\n", FilenameSize); // 负值表示UTF-16

                    fprintf(logFile, "Filename (UTF-8): %s\n", Filename);



                    // 打印 FileHash FileOffset FileSize CompressionMethod CompressedLength Dummy CompressedBlockSize Encrypted 等原始数据键值对

                    fprintf(logFile, "\n--- 原始数据键值对 ---\n");

                    

                    // 打印 FileHash (20字节数组)

                    fprintf(logFile, "FileHash: ");

                    for (int i = 0; i < 20; i++)

                    {

                        fprintf(logFile, "%02x", entry[ENTRY].FileHash[i]);

                    }

                    fprintf(logFile, "\n");

                    

                    fprintf(logFile, "FileOffset: 0x%llx (%llu)\n", entry[ENTRY].FileOffset, entry[ENTRY].FileOffset);

                    fprintf(logFile, "FileSize (Original): %llu\n", entry[ENTRY].FileSize);

                    fprintf(logFile, "CompressionMethod: %u\n", entry[ENTRY].CompressionMethod);

                    fprintf(logFile, "CompressedLength: %llu\n", entry[ENTRY].CompressedLength);

                    

                    // 打印 Dummy (21字节数组)

                    fprintf(logFile, "Dummy: ");

                    for (int i = 0; i < 21; i++)

                    {

                        fprintf(logFile, "%02x", entry[ENTRY].Dummy[i]);

                    }

                    fprintf(logFile, "\n");



                    fprintf(logFile, "NumOfBlocks: %u\n", entry[ENTRY].NumOfBlocks);

                    for (uint32_t i = 0; i < entry[ENTRY].NumOfBlocks; i++)

                    {

                        // 打印每个压缩块的起始和结束偏移

                        fprintf(logFile, "Block %u: Start = 0x%llx, End = 0x%llx\n", i, entry[ENTRY].blocks[i].start, entry[ENTRY].blocks[i].end);  

                    }

                    fprintf(logFile, "CompressedBlockSize: %u\n", entry[ENTRY].CompressedBlockSize);

                    fprintf(logFile, "Encrypted: %u\n", entry[ENTRY].Encrypted);



                    fclose(logFile);

                }



                printf("Found target file: %s, ENTRY: %d. Information saved to target_file_info.txt.\n", path, ENTRY);

            }



            // 调用提取函数，传入文件元数据和路径

            // extract(PakFile, entry[ENTRY], path);

        }

    }



根据以上读取《Directory Map》的代码，修改以下《 写入 Directory Map》的代码，改为在原版《Directory Map》的基础上进行寻找指定修改，再进行写入缓存。先根据ENTRY Index == src_data.NumOfEntry -1 和ENTRY Index == src_data.NumOfEntry - 2来找到需要更改的这两个《Directory Map》，然后更改其DIR_LEN DIR_NAME DIR_FILES  FilenameSize Filename，最后再进行写入缓存。



// 4.3. 写入 Directory Map

    uint64_t dir_map_size = src_data.OriginalIndexSize - src_data.EntryListEndOffset;

    write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData + src_data.EntryListEndOffset, dir_map_size);


剩余工作：1、Directory Map的修改  2、元数据103字节差异的探索