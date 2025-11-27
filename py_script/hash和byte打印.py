'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-11-24
Description  : 比较两个 PAK 文件的大小与哈希，并解析两个指定偏移处的 UE4 Pak Entry 头部。
Copyright (c) 2025 by vitalchem, All Rights Reserved.
'''
from pathlib import Path
import hashlib
import struct

# 配置参数
start_byte = 306190
print_byte_size = 94+4
print_byte_size_pre = 0
print_byte_size_after = 0

src_pak = Path("./paks/game_patch_1.34.12.14515验证ace稳定1127.pak")
my_pak = Path("./paks/game_patch_1.34.12.14515验证ace稳定1127.pak")

def compute_file_hash(file_path: Path, algorithm='sha256') -> str:
    """计算文件的哈希值"""
    hash_func = hashlib.new(algorithm)
    with open(file_path, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_func.update(chunk)
    return hash_func.hexdigest()

def get_file_size(file_path: Path) -> int:
    """获取文件字节大小"""
    return file_path.stat().st_size

def print_hex_region(file_path: Path, offset: int, pre: int, size: int, post: int):
    """打印 offset 前后指定字节数的十六进制内容"""
    total_read = pre + size + post
    read_offset = offset - pre
    if read_offset < 0:
        # 如果偏移太小，调整到文件开头
        adjust = -read_offset
        read_offset = 0
        pre -= adjust
        total_read -= adjust
        print(f"⚠️  Warning: offset {offset} is too close to start; adjusted pre bytes to {pre}")

    try:
        with open(file_path, 'rb') as f:
            f.seek(read_offset)
            data = f.read(total_read)
    except OSError as e:
        print(f"❌ Error reading {file_path.name}: {e}")
        return

    # 打印十六进制
    print(f"\n--- Hex dump of {file_path.name} around offset 0x{offset:X} ({offset}) ---")
    print(f"Range: [{read_offset} : {read_offset + len(data)}] (total {len(data)} bytes)")
    print("Offset(h) | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | ASCII")
    print("-" * 70)

    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_part = ' '.join(f"{b:02X}" for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
        print(f"{read_offset + i:08X}  | {hex_part:<47} | {ascii_part}")

# --- 主逻辑 ---
if not src_pak.exists():
    raise FileNotFoundError(f"Source PAK not found: {src_pak}")
if not my_pak.exists():
    raise FileNotFoundError(f"My PAK not found: {my_pak}")

# 1. 比较文件大小
size_src = get_file_size(src_pak)
size_my = get_file_size(my_pak)
print(f"📄 File Sizes:")
print(f"  {src_pak.name}: {size_src:,} bytes")
print(f"  {my_pak.name}:   {size_my:,} bytes")
print(f"  ➤ Size Match: {'✅ Yes' if size_src == size_my else '❌ No'}")

# 2. 比较哈希值（SHA256）
print("\n🔐 Hash Comparison (SHA256):")
hash_src = compute_file_hash(src_pak)
hash_my = compute_file_hash(my_pak)
print(f"  {src_pak.name}: {hash_src}")
print(f"  {my_pak.name}:   {hash_my}")
print(f"  ➤ Hash Match: {'✅ Yes' if hash_src == hash_my else '❌ No'}")

# 3. 打印两个文件在指定位置的十六进制内容
target_offset = start_byte
print(f"\n🔍 Comparing hex content around offset {target_offset} (0x{target_offset:X})")

print_hex_region(src_pak, target_offset, print_byte_size_pre, print_byte_size, print_byte_size_after)
print_hex_region(my_pak, target_offset, print_byte_size_pre, print_byte_size, print_byte_size_after)
