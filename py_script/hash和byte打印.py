'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-11-24
Description  : 比较两个 PAK 文件的大小与哈希，并解析两个指定偏移处的 UE4 Pak Entry 头部。
Copyright (c) 2025 by vitalchem, All Rights Reserved.
'''
from pathlib import Path
import hashlib
import struct

# === 配置 ===
OFFSET_ni0 = 2321236          # ni0 对应的 FileOffset
OFFSET_ni1 = 2327138          # ni1 对应的 FileOffset 原版

HEAD_SIZE = 94                # UE4 Pak Entry 头部固定长度（字节）

# SRC_PAK = Path("./paks/game_patch_1.33.12.14429原厂（复件）.pak")
SRC_PAK = Path("./paks/map_weapon_1.34.12.14500原厂.pak")
NEW_PAK = Path("./paks/map_weapon_1.34.12.14500原厂.pak")

# 定义要检查的偏移点：(标签, 偏移值)
offsets_to_check = [
    ("ni0", OFFSET_ni0),
    ("ni1", OFFSET_ni1),
]


def compute_file_hash(path: Path, algorithm='sha256') -> str:
    """计算文件的 SHA256（或其他算法）哈希值"""
    h = hashlib.new(algorithm)
    with open(path, 'rb') as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def parse_pak_head(data: bytes):
    """解析 94 字节的 UE4 Pak Entry 头部结构"""
    if len(data) < HEAD_SIZE:
        raise ValueError("数据长度不足 94 字节")
    
    off = 0
    result = {}
    # 1. FileHash (20 bytes)
    result['FileHash'] = data[off:off+20].hex()
    off += 20
    # 2. FileOffset (8 bytes, little-endian)
    result['FileOffset'] = struct.unpack('<Q', data[off:off+8])[0]
    off += 8
    # 3. FileSize (8 bytes)
    result['FileSize'] = struct.unpack('<Q', data[off:off+8])[0]
    off += 8
    # 4. CompressionMethod (4 bytes)
    result['CompressionMethod'] = struct.unpack('<I', data[off:off+4])[0]
    off += 4
    # 5. CompressedLength (8 bytes)
    result['CompressedLength'] = struct.unpack('<Q', data[off:off+8])[0]
    off += 8
    # 6. Dummy padding (21 bytes)
    off += 21
    # 7. NumOfBlocks (4 bytes)
    num_blocks = struct.unpack('<I', data[off:off+4])[0]
    result['NumOfBlocks'] = num_blocks
    off += 4

    # 8. Blocks (each block: start Q, end Q)
    blocks = []
    for _ in range(num_blocks):
        start = struct.unpack('<Q', data[off:off+8])[0]
        off += 8
        end = struct.unpack('<Q', data[off:off+8])[0]
        off += 8
        blocks.append((start, end))
    result['Blocks'] = blocks

    # 9. CompressedBlockSize (4 bytes)
    result['CompressedBlockSize'] = struct.unpack('<I', data[off:off+4])[0]
    off += 4
    # 10. Encrypted flag (1 byte)
    result['Encrypted'] = data[off]
    off += 1

    assert off == HEAD_SIZE, f"解析长度错误：期望 {HEAD_SIZE}，实际 {off}"
    return result


def read_bytes_at(path: Path, offset: int, size: int) -> bytes:
    """从文件指定偏移读取指定字节数"""
    with open(path, 'rb') as f:
        f.seek(offset)
        return f.read(size)


# === 主流程 ===
if not SRC_PAK.exists():
    raise FileNotFoundError(f"源 PAK 文件不存在: {SRC_PAK}")
if not NEW_PAK.exists():
    raise FileNotFoundError(f"新 PAK 文件不存在: {NEW_PAK}")

# 1. 比较文件大小与哈希
size_src = SRC_PAK.stat().st_size
size_new = NEW_PAK.stat().st_size
hash_src = compute_file_hash(SRC_PAK)
hash_new = compute_file_hash(NEW_PAK)

print("📄 文件大小对比:")
print(f"  原版: {size_src:,} 字节")
print(f"  新版: {size_new:,} 字节")
print(f"  是否一致: {'✅ 是' if size_src == size_new else '❌ 否'}")

print("\n🔐 SHA256 哈希值:")
print(f"  原版: {hash_src}")
print(f"  新版: {hash_new}")
print(f"  是否一致: {'✅ 是' if hash_src == hash_new else '❌ 否'}")

# 2. 解析两个偏移处的头部
for label, offset in offsets_to_check:
    print(f"\n🔍 正在解析 {label}（偏移 {offset} / 0x{offset:X}）处的 94 字节头部...")

    for name, path in [("原版", SRC_PAK), ("新版", NEW_PAK)]:
        try:
            data = read_bytes_at(path, offset, HEAD_SIZE)
            if len(data) < HEAD_SIZE:
                print(f"\n⚠️  {name} PAK 在偏移 {offset} 处仅读取到 {len(data)} 字节，不足 94 字节！")
                continue
            parsed = parse_pak_head(data)
            print(f"\n--- {name} PAK - {label} 头部信息 ---")
            for key, value in parsed.items():
                print(f"{key}: {value}")
        except Exception as e:
            print(f"\n❌ 解析 {name} PAK 的 {label} 头部失败: {e}")