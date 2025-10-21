#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
auto_skin_final.py
功能：
 1. 直接处理指定 dat 文件
 2. 先做一次 swap 美化
 3. 再批量替换 ID → 405017
 4. 打印实时进度
"""

import re
import mmap
from pathlib import Path
from typing import List, Tuple

# 最终合并、去重并排序后的裤子、裙子ID列表 (共 267 个 ID) 专门排除了 404007 竞技裤(黑)- 404040, 404041 特训学院裤 创建角色已占用 防止self替换self
PANTS_ID_LIST = [
    404001, 404002, 404003, 404004, 404005, 
    404006, 404008, 404009, 404010, 
    404011, 404012, 404013, 404014, 404015, 
    404016, 404023, 404024, 404025, 404026, 
    404027, 404028, 404029, 404030, 404031, 
    404032, 404033, 404034, 404035, 404036, 
    404037, 404038, 404039, 
    404042, 404043, 404044, 404045, 404046, 
    404047, 404048, 404049, 404050, 404051, 
    404052, 404053, 404054, 404055, 404056, 
    404057, 404058, 404059, 404060, 404061, 
    404062, 404063, 404064, 404065, 404066, 
    404067, 404068, 404069, 404070, 404071, 
    404072, 404073, 404074, 404075, 404076, 
    404077, 404078, 404079, 404080, 404081, 
    404082, 404083, 404084, 404085, 404086, 
    404087, 404088, 404089, 404090, 404091, 
    404092, 404093, 404094, 404095, 404096, 
    404097, 404098, 404099, 404100, 404101, 
    404102, 404103, 404104, 404105, 404106, 
    404107, 404108, 404109, 404110, 404111, 
    404112, 404113, 404115, 404116, 404117, 
    404118, 404119, 404120, 404121, 404122, 
    404123, 404124, 404125, 404126, 404127, 
    404128, 404129, 404130, 404131, 404132, 
    404133, 404134, 404135, 404136, 404137, 
    404138, 404139, 404141, 404142, 404143, 
    404144, 404145, 404146, 404147, 404148, 
    404149, 404150, 404151, 404152, 404153, 
    404154, 404155, 404156, 404157, 404158, 
    404159, 404160, 404161, 404162, 404163, 
    404164, 404165, 404166, 404167, 404168, 
    404169, 404170, 404171, 404172, 404173, 
    404174, 404175, 404176, 404177, 404178, 
    404179, 404180, 404181, 404182, 404183, 
    404184, 404185, 404186, 404187, 404188, 
    404189, 404190, 404191, 404192, 404193, 
    404194, 404195, 404196, 404197, 404198, 
    404199, 404200, 404201, 404202, 404203, 
    404204, 404205, 404206, 404207, 404208, 
    404209, 404210, 404211, 404212, 404213, 
    404214, 404215, 404216, 404217, 404218, 
    404219, 404220, 404221, 404222, 404223, 
    404224, 404225, 404226, 404227, 404228, 
    404229, 404230, 404231, 404232, 404233, 
    404234, 404235, 404236, 404237, 404238, 
    404239, 404240, 404241, 404242, 404243, 
    404244, 404245, 404247, 404248, 404249, 
    404250, 404251, 404252, 404253, 404254, 
    404255, 404256, 404257, 404258, 404259, 
    404260, 404261, 404262, 404263, 404264, 
    404265, 404266, 404267, 404268, 404269, 
    404270, 404271, 404272, 404274, 404275, 
    404276, 404277, 404278, 404279, 404280, 
    404281, 404282, 404283, 404284, 404285, 
    404286, 404288, 404289, 404290, 404291, 
    404292, 404293, 404294, 404295, 404296, 
    404297, 404298, 404299, 404300, 404301, 
    404302, 404303, 404304, 404305, 404306, 
    404307, 404308, 404309, 404310, 404311, 
    404312, 404313, 404315, 404316, 404317, 
    404318, 404319, 404320, 404321, 404322, 
    404323, 404324, 404325, 404326, 404327, 
    404328, 404329, 404330, 404331, 404332, 
    404333, 404334, 404335, 404336, 404337, 
    404338, 404339, 404340, 404341, 404342, 
    404343, 404346, 404347, 404348, 404349, 
    404352, 404353, 404354, 404355, 404356, 
    404357, 404359, 404360, 404361, 404362, 
    454001, 454003, 454004, 454005, 454006, 
    454007, 474001, 474002, 474003, 474004, 
    474005, 474006, 474007, 474008, 474009
]

# 最终合并、去重并排序后的鞋子ID列表 (共 412 个 ID) 专门排除了405017特训学院靴子 和 405011棕鞋 防止self替换self
SHOES_ID_LIST = [ 
    405001, 405002, 405004, 405005, 405006, 
    405007, 405008, 405009, 405010, 
    405012, 405013, 405014, 405015, 405016, 
    405018, 405019, 405020, 405022, 
    405023, 405024, 405025, 405026, 405027, 
    405029, 405032, 405033, 405034, 405035, 
    405036, 405037, 405038, 405039, 405040, 
    405041, 405043, 405044, 405048, 405049, 
    405050, 405051, 405052, 405053, 405054, 
    405055, 405056, 405057, 405059, 405062, 
    405064, 405065, 405067, 405069, 405070, 
    405071, 405072, 405073, 405074, 405075, 
    405076, 405079, 405080, 405086, 405090, 
    405091, 405092, 405093, 405096, 405098, 
    405099, 405100, 405101, 405102, 405103, 
    405104, 405105, 405106, 405107, 405108, 
    405109, 405110, 405111, 405112, 405113, 
    405114, 405115, 405116, 405117, 405119, 
    405120, 405121, 405122, 405123, 405124, 
    405125, 405126, 405127, 405128, 405129, 
    405130, 405131, 405132, 405133, 405134, 
    405135, 405136, 405137, 405138, 405139, 
    405140, 405141, 405142, 405143, 405144, 
    405145, 405146, 405147, 405148, 405149, 
    405150, 405151, 405152, 405153, 405155, 
    405156, 405157, 405158, 405159, 405160, 
    405161, 405162, 405163, 405164, 405165, 
    405166, 405167, 405168, 405169, 405170, 
    405172, 405173, 405174, 405175, 405176, 
    405177, 405178, 405179, 405180, 405181, 
    405182, 405183, 405184, 405185, 405186, 
    405187, 405188, 405189, 405190, 405191, 
    405192, 405193, 405194, 405195, 405196, 
    405197, 405199, 405200, 405201, 405202, 
    405203, 405204, 405205, 405206, 405207, 
    405208, 405209, 405210, 405211, 405212, 
    405214, 405215, 405216, 405217, 405218, 
    405219, 405220, 405221, 405222, 405223, 
    405224, 405225, 405226, 405227, 405228, 
    405229, 405230, 405231, 405232, 405233, 
    405234, 405235, 405236, 405237, 405238, 
    405239, 405240, 405241, 405243, 405244, 
    405245, 405246, 405247, 405248, 405249, 
    405250, 405251, 405252, 405253, 405254, 
    405255, 405256, 405257, 405258, 405259, 
    405260, 405261, 405262, 405263, 405264, 
    405265, 405266, 405267, 405268, 405269, 
    405270, 405271, 405272, 405273, 405274, 
    405275, 405276, 405277, 405278, 405279, 
    405280, 405281, 405282, 405283, 405284, 
    405285, 405286, 405287, 405288, 405289, 
    405290, 405291, 405292, 405293, 405296, 
    405297, 405298, 405299, 405301, 405302, 
    405303, 405304, 405305, 405306, 405307, 
    405308, 405309, 405310, 405311, 405313, 
    405314, 405315, 405316, 405317, 405318, 
    405319, 405320, 405321, 405322, 405323, 
    405324, 405325, 405326, 405327, 405328, 
    405329, 405330, 405331, 405332, 405333, 
    405334, 405335, 405336, 405337, 405338, 
    405339, 405340, 405341, 405344, 405345, 
    405346, 405347, 405349, 405352, 405353, 
    405354, 405355, 405356, 405357, 405358, 
    405359, 405360, 405361, 405362, 405364, 
    405365, 405366, 405367, 405369, 405371, 
    405373, 405374, 405375, 405377, 405378, 
    405379, 405385, 405396, 405397, 405398, 
    405399, 405401, 405402, 405403, 405405, 
    405406, 405407, 405408, 405409, 405410, 
    405411, 405412, 405413, 405414, 405415, 
    405416, 405417, 405418, 405419, 405420, 
    405421, 405422, 405423, 405424, 405425, 
    405426, 405427, 405428, 405429, 405430, 
    405431, 405432, 405433, 405434, 405435, 
    405436, 405437, 405439, 405440, 405441, 
    405442, 405443, 405444, 455002, 455003, 
    812018, 812019, 812020, 815028, 815033, 
    815037]

# 我仓库的鞋子ID列表 (共 15 个 ID)
ID_LIST_MY = [
    405017,405036,405040,405043,405059,
    405055,405196,405100,405116,405131,
    405176,405221,405222,405124,405117]
"""仓库已有的鞋子：
特训学院靴子  青年教官鞋子  橘黑制服鞋子  深蓝色名流皮鞋  红色运动鞋
白色伏地魔鞋子  滨海假日凉鞋 黄色名流皮鞋  热血青春竞技靴  千禧之恋靴子  
战地督导靴子  丛林猎人鞋  缤纷果语凉鞋 魔术贴红色网球鞋 魔术贴紫色网球鞋
"""
# 天线美化。核心替换项
SWAP_CONFIG_CORE = [
    (404007, 413619),   # 竞技裤(黑)-创建角色已占用 <=> 幽焰骑士1 413619 
    (405011, 413739),   # 棕色高帮运动鞋-创建角色已占用 <=> 幽焰骑士2 413739
    (812018, 413495),    # 默认鞋子(女) ↔  关羽3
    (812019, 413494),    # 默认鞋子(男) ↔ 关羽2
    (812020, 413820),    # 默认鞋子(通用) ↔ 瑞拉3 
    (405001, 413851),    # 运动鞋(白) ↔ 金蛇镇世-烛九3
    (404110, 413063),     # 牛仔裤(蓝) ↔ 初号机款机体服
    (403251, 413495)    # T恤(白)(新)(创建角色使用) <=> 宇宙3
]
# 天线美化。扩展替换项
SWAP_CONFIG_PLUGIN = [

    (802397, 413508),    # 背包挂件-扫描仪 <=>  沙丘3 异瞳寒姬 430354   紫俏灵猫 绯色魅影
    (503001, 413740),    # 1级甲 <=> 幽焰骑士3 
    (503002, 413497),    # 2级甲 <=> 赵云2 413497
    (503003, 413498)     # 3级甲 <=> 赵云3 413498

]

# ============ 工具函数 ============

def swap_in_dat(dat_path: Path, swaps: List[Tuple[int, int]]):
    """
    在 dat 文件中进行 ID 对调（swap）
    """
    print(f"[STEP] 开始 swap 操作 -> {dat_path}")
    with dat_path.open("r+b") as fp:
        data = fp.read()
        for old_val, new_val in swaps:
            old_str = str(old_val).encode()
            new_str = str(new_val).encode()
            # 匹配前后固定长度 只匹配第一个搜索结果
            pattern_old = rb'(.{8})' + re.escape(old_str) + rb'(.{10})'
            pattern_new = rb'(.{8})' + re.escape(new_str) + rb'(.{10})'

            # DEBUG 找到所有匹配
            matches_old = list(re.finditer(pattern_old, data, flags=re.DOTALL))
            matches_new = list(re.finditer(pattern_new, data, flags=re.DOTALL))
            # DEBUG 打印所有匹配上下文
            print(f"  [INFO] old ID {old_val} 匹配块:")
            for m in matches_old:
                print(f"    offset={m.start():06}  block={to_hex(m.group(0))}")

            print(f"  [INFO] new ID {new_val} 匹配块:")
            for m in matches_new:
                print(f"    offset={m.start():06}  block={to_hex(m.group(0))}")

            # 搜索块
            m_old = re.search(pattern_old, data)
            m_new = re.search(pattern_new, data)
            if not m_old or not m_new:
                print(f"  [WARN] 未找到 ID {old_val} 或 {new_val} 的数据块")
                continue
            block_old = m_old.group(0)
            block_new = m_new.group(0)
            data = data.replace(block_old, b"__TMP__")
            data = data.replace(block_new, block_old)
            data = data.replace(b"__TMP__", block_new)
            print(f"  [OK] 已交换 ID {old_val} ↔ {new_val}")
        fp.seek(0)
        fp.write(data)
        fp.truncate()
    print("[DONE] swap 完成\n")

def to_hex(b: bytes) -> str:
    """字节转十六进制字符串，每字节两位，用空格分隔"""
    return " ".join(f"{x:02X}" for x in b)
def patch_ids(dat_path: Path, replacements: List[Tuple[str, str]]):
    """
    批量替换 dat 文件中的 ID，并加固定上下文字节（前3字节00，后1字节00）
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    print(f"[STEP] 开始批量替换 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_str, new_str in replacements:
                # 转成字节
                old_id_bytes = str(old_str).encode("utf-8")
                new_id_bytes = str(new_str).encode("utf-8")

                # 上下文字节
                prefix = b"\x00\x00\x00"
                suffix = b"\x00"

                # 整个匹配块
                old_bytes = prefix + old_id_bytes + suffix
                new_bytes = prefix + new_id_bytes + suffix

                if len(old_bytes) != len(new_bytes):
                    print(f"  [WARN] 跳过长度不同的替换 {old_str} → {new_str}")
                    continue

                pos = 0
                replaced = 0
                while True:
                    pos = mm.find(old_bytes, pos)
                    if pos == -1:
                        break

                    # 打印上下文：前12字节+ID+后12字节
                    start = max(pos - 12, 0)
                    end = min(pos + len(old_bytes) + 12, mm.size())
                    context_bytes = mm[start:end]
                    # print(f"  [MATCH] offset={pos:06} | 12+ID+12: {to_hex(context_bytes)}")

                    # 替换
                    mm[pos:pos+len(old_bytes)] = new_bytes
                    replaced += 1
                    pos += len(old_bytes)

                if replaced > 1:
                    print(f"  [OK] {old_str} ({to_hex(old_bytes)}) → {new_str} ({to_hex(new_bytes)}) 替换 {replaced} 次")
        finally:
            mm.close()
    print("[DONE] 批量替换完成\n")

def patch_ptrs_by_ids(dat_path: Path, replacements: List[Tuple[str, str]]):
    """
    批量替换 dat 文件中的指针：
    根据 (old_id, new_id) 配置，将 old_id 的指针替换为 new_id 的指针
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    # 上下文字节
    prefix = b"\x00\x00\x00"
    suffix = b"\x00"

    print(f"[STEP] 开始批量替换指针 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_id, new_id in replacements:
                old_id_bytes = old_id.encode("utf-8")
                new_id_bytes = new_id.encode("utf-8")

                # 构造块
                old_block = prefix + old_id_bytes + suffix
                new_block = prefix + new_id_bytes + suffix

                # 先找到 new_id 的指针
                pos_new = mm.find(new_block)
                if pos_new == -1:
                    print(f"  [WARN] 未找到 new_id={new_id}")
                    continue

                new_ptr_start = pos_new + len(new_block)
                new_ptr = mm[new_ptr_start:new_ptr_start+5]

                print(f"  [INFO] new_id={new_id} 指针={to_hex(new_ptr)} (offset={new_ptr_start})")

                # 替换 old_id 的所有指针
                pos = 0
                replaced = 0
                while True:
                    pos = mm.find(old_block, pos)
                    if pos == -1:
                        break

                    old_ptr_start = pos + len(old_block)
                    old_ptr = mm[old_ptr_start:old_ptr_start+5]

                    # 打印上下文
                    start = max(pos - 12, 0)
                    end = min(old_ptr_start + 5 + 12, mm.size())
                    context_bytes = mm[start:end]
                    print(f"  [MATCH] old_id={old_id} offset={pos:06} | {to_hex(context_bytes)}")

                    # 替换指针
                    mm[old_ptr_start:old_ptr_start+5] = new_ptr
                    replaced += 1
                    pos = old_ptr_start + 5

                if replaced > 0:
                    print(f"  [OK] old_id={old_id} ({to_hex(old_ptr)}) → new_id={new_id} ({to_hex(new_ptr)}) 替换 {replaced} 次")
        finally:
            mm.close()
    print("[DONE] 批量指针替换完成\n")

def swap_id_and_ptr_in_dat(dat_path: Path, swaps: List[Tuple[str, str]]):
    """
    在 dat 文件中互换 old_id 和 new_id 的 ID 与指针
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    prefix = b"\x00\x00\x00"
    suffix = b"\x00"

    print(f"[STEP] 开始互换 ID+指针 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_id, new_id in swaps:
                old_block = prefix + old_id.encode("utf-8") + suffix
                new_block = prefix + new_id.encode("utf-8") + suffix

                pos_old = mm.find(old_block)
                pos_new = mm.find(new_block)
                if pos_old == -1 or pos_new == -1:
                    print(f"  [WARN] 未找到 old_id={old_id} 或 new_id={new_id}")
                    continue

                old_ptr_start = pos_old + len(old_block)
                new_ptr_start = pos_new + len(new_block)
                old_ptr = mm[old_ptr_start:old_ptr_start+5]
                new_ptr = mm[new_ptr_start:new_ptr_start+5]

                # 构造完整块（ID+指针）
                block_old = old_block + old_ptr
                block_new = new_block + new_ptr

                # 长度必须相等才能交换
                if len(block_old) != len(block_new):
                    print(f"  [WARN] old_id={old_id}, new_id={new_id} 块长度不同，跳过")
                    continue

                # 逐字节互换
                mm[pos_old:pos_old+len(block_old)] = block_new
                mm[pos_new:pos_new+len(block_new)] = block_old

                print(f"  [OK] 已互换 old_id={old_id}, new_id={new_id} (包含指针)")
        finally:
            mm.close()
    print("[DONE] 互换 ID+指针 完成\n")

def swap_ptr_in_dat(dat_path: Path, swaps: List[Tuple[str, str]]):
    """
    在 dat 文件中互换 old_id 和 new_id 的指针（仅交换指针，不动 ID）
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    prefix = b"\x00\x00\x00"
    suffix = b"\x00"

    print(f"[STEP] 开始互换指针 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_id, new_id in swaps:
                old_block = prefix + old_id.encode("utf-8") + suffix
                new_block = prefix + new_id.encode("utf-8") + suffix

                pos_old = mm.find(old_block)
                pos_new = mm.find(new_block)
                if pos_old == -1 or pos_new == -1:
                    print(f"  [WARN] 未找到 old_id={old_id} 或 new_id={new_id}")
                    continue

                old_ptr_start = pos_old + len(old_block)
                new_ptr_start = pos_new + len(new_block)
                old_ptr = mm[old_ptr_start:old_ptr_start+5]
                new_ptr = mm[new_ptr_start:new_ptr_start+5]

                if len(old_ptr) != len(new_ptr):
                    print(f"  [WARN] old_id={old_id}, new_id={new_id} 指针长度不同，跳过")
                    continue

                # 交换指针
                mm[old_ptr_start:old_ptr_start+5] = new_ptr
                mm[new_ptr_start:new_ptr_start+5] = old_ptr

                print(f"  [OK] 已互换 old_id={old_id}, new_id={new_id} 的指针")
        finally:
            mm.close()
    print("[DONE] 互换指针完成\n")

# ============ 主流程 ============
def main():
    # 直接指定要处理的 dat 文件
    target_dat = Path("./release/RE天线V4发布20250930/00000498原厂（复件）.dat")

    # 第一次 swap 配置
    # swap_config = [(403251, 413497), (405011, 413498)] #  白T <=> 赵云2 。棕鞋 <=> 赵云3
    # swap_config = [(404007, 413497), (405011, 413498),(403251, 413657)] #  竞技裤（黑色） <=> 赵云2 。棕鞋 <=> 赵云3 。白T恤 <=> 宇宙3
    # swap_config = [(403251, 413497)] #  仅白T <=> 赵云2。效果：白色天线常亮+黑色天线呼吸灯 透明衣
    # swap_config = [(403251, 413494), (405011, 413497)] #  白T <=> 关羽2 。棕鞋 <=> 赵云2 。效果：
    # swap_config = [(403251, 413507), (405011, 413498)] #  白T <=> 沙丘主。棕鞋 <=> 赵云3 。效果：
    # swap_config = [(403251, 423099), (405011, 423100)] #  白T <=> 哪吒2。棕鞋 <=> 哪吒3 。效果：
    # swap_config = [(403251, 413497), (503003, 413498)] #  白T <=> 赵云2 。三级甲 <=> 赵云3


    # swap_in_dat(target_dat, swap_config) # 仅交换ID

    SWAP_CONFIG_CORE_str = [(str(old), str(new)) for old, new in SWAP_CONFIG_CORE] 
    swap_id_and_ptr_in_dat(target_dat, SWAP_CONFIG_CORE_str) # 交换ID、指针 3组核心交换
    # swap_ptr_in_dat(target_dat, swap_config_str)

    # # swap_config_my = list(zip(ID_LIST_MY, ID_LIST_SWAP))
    # swap_config_my = swap_config_my[1:]  # 跳过第1组，从第2组开始
    # swap_config_my = swap_config_my[:3]  # 只保留前3组，跳过之后的所有
    # swap_config_my = swap_config_my[::3]  # 每隔 3 个取 1 个（0, 3, 6, 9, 12...）
    # swap_config_my = swap_config_my[2::3]  # 从索引2开始，每隔3个取1个（2,5,8,11,...）
    # swap_config_my_str = [(str(old), str(new)) for old, new in swap_config_my] 
    SWAP_CONFIG_PLUGIN_str = [(str(old), str(new)) for old, new in SWAP_CONFIG_PLUGIN] 
    swap_id_and_ptr_in_dat(target_dat, SWAP_CONFIG_PLUGIN_str) # 交换ID、指针 n组扩展交换

    # 第二次批量替换配置
    # target_code = "405009" # 红色高帮运动鞋
    # target_code = "405017" # 特训学院靴子 古法
    # target_code = "405011"   # 棕鞋 效果：很多人都是 透明鞋 透明衣
    # target_code = "423100"   # 哪吒2 效果：很多人都是 透明鞋 透明衣
    # target_code = "413496"   # 赵云2 效果：
    # target_code = "413820"   # 瑞拉3 效果：
    # target_code = "404040" # 特训学院下装 裤子 新法 隐藏自身天线 没用

    # replace_config = [(str(old), target_code) for old in PANTS_ID_LIST]
    # patch_ids(target_dat, replace_config) # 仅搜索并替换ID

    # replace_config = [(str(old), str(new)) for old, new in [(405017, 413820),(405059, 413498), (503002, 413498), (503003, 413498)]]
    # patch_ids(target_dat, replace_config)

    # 413701 -- 无面战甲角色  430359  角色-无面战甲

    # replace_config = [(str(old), str(new)) for old, new in [(413701, 413497),(430359, 413497), # 无面战甲
    #                                                         (403251, 413498),(405011, 413498), # 白T棕鞋
    #                                                         (503001, 413498), (503002, 413498), (503003, 413498), # 123级甲
    #                                                         (405017, 413498), (812018, 413498), (812019, 413498),(812020, 413498)]] # 特训鞋，默认3鞋
    # replace_config = [(str(old), str(new)) for old, new in [(413497, 403251),(413498, 405011)]] # 白T棕鞋
    # patch_ptrs_by_ids(target_dat, swap_config_my_str)
    # patch_ids(target_dat, swap_config_my_str)
    print("[ALL DONE] 处理完成！结果已写回:", target_dat)

if __name__ == "__main__":
    main()
