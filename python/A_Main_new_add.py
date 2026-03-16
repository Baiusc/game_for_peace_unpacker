import os
import shutil
import struct
import hashlib
import zlib  # 引入 zlib 压缩库
import B_PackTool_quickbms as pt

WATERMARK = """
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
  老 6 工 具 - V10 资产无缝注入 (Zip=3 压缩版) 
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
"""

# 和平精英专属密钥
XOR_KEY = 0x79
OFFSET_KEY = 0xD74AF37FAA6B020D
MAX_CHUNK_SIZE = 65536  # UE4 官方标准压缩块大小 64KB

def xor_bytes(data: bytes, key: int = XOR_KEY) -> bytes:
    """简单的按位异或加密/解密"""
    return bytes([b ^ key for b in data])

def serialize_ue4_string(s: str) -> bytes:
    """序列化 UE4 字符串格式: Length(4字节) + 字符串内容 + \\x00"""
    encoded = s.encode('utf-8') + b'\x00'
    length = len(encoded)
    return struct.pack('<i', length) + encoded

def compress_and_create_fpakentry(file_offset: int, uncompressed_data: bytes) -> tuple:
    """
    【核心改造】构造 UE4 标准的 Zip=3 (Zlib) 格式的 FPakEntry 头与压缩数据
    返回: (meta_bytes, compressed_payload_bytes)
    """
    uncompressed_size = len(uncompressed_data)
    compressed_chunks = []
    
    # 1. 按照 MAX_CHUNK_SIZE (64KB) 对数据进行切片并 zlib 压缩
    for i in range(0, uncompressed_size, MAX_CHUNK_SIZE):
        raw_chunk = uncompressed_data[i : i + MAX_CHUNK_SIZE]
        comp_chunk = zlib.compress(raw_chunk, level=9) # 使用最高压缩率，保证与官方一致
        compressed_chunks.append(comp_chunk)
        
    chunk_count = len(compressed_chunks)
    
    # 2. 计算 Meta 结构占用的大小，以推导第一个 Chunk 的起始绝对偏移
    # Base(69) + ChunkCount(4) + Chunks(16 * count) + MaxChunkSize(4) + Encrypted(1)
    meta_size = 69 + 4 + (16 * chunk_count) + 5
    payload_start_offset = file_offset + meta_size
    
    # 3. 计算每个压缩块的 Start 和 End 偏移量，以及总压缩后体积 ZSize
    chunk_offsets = []
    current_offset = payload_start_offset
    zsize = 0
    for comp_chunk in compressed_chunks:
        c_len = len(comp_chunk)
        chunk_offsets.append((current_offset, current_offset + c_len))
        current_offset += c_len
        zsize += c_len
        
    data_hash = hashlib.sha1(uncompressed_data).digest()

    # 4. 组装 FPakEntry 字节流
    meta = bytearray()
    meta.extend(data_hash)                     # 20 bytes: Hash (未压缩数据的哈希)
    meta.extend(struct.pack('<q', file_offset))# 8 bytes: Offset (文件的绝对起始偏移)
    meta.extend(struct.pack('<q', uncompressed_size)) # 8 bytes: Size (解压后总大小)
    meta.extend(struct.pack('<i', 3))          # 4 bytes: Zip = 3 (Zlib压缩格式)
    meta.extend(struct.pack('<q', zsize))      # 8 bytes: ZSize (压缩后总大小)
    meta.extend(b'\x00' * 21)                  # 21 bytes: Dummy
    
    # --- Chunks 记录区 ---
    meta.extend(struct.pack('<i', chunk_count))# 4 bytes: 压缩块数量
    for start_off, end_off in chunk_offsets:
        meta.extend(struct.pack('<q', start_off)) # 8 bytes: Chunk 起始偏移
        meta.extend(struct.pack('<q', end_off))   # 8 bytes: Chunk 结束偏移
        
    # --- Tail 尾部区 ---
    meta.extend(struct.pack('<i', MAX_CHUNK_SIZE)) # 4 bytes: MaxChunkSize (65536)
    meta.extend(b'\x00')                           # 1 byte: Encrypted = 0

    compressed_payload = b''.join(compressed_chunks)
    return bytes(meta), compressed_payload

def extract_asset(tool: pt.UE4PakEngine, pak_path: str, ext: str, asset_base_name: str):
    """从高版本提取池中提取资产数据"""
    target_idx = -1
    target_path = ""
    search_str = asset_base_name + ext
    for path, idx in tool.files_map.items():
        if path.endswith(search_str):
            target_idx = idx
            target_path = path
            break
            
    if target_idx == -1:
        return None, None
        
    e = tool.entries_meta[target_idx]
    with open(pak_path, 'rb') as f:
        if len(e['chunks']) > 0:
            data = bytearray()
            for chunk in e['chunks']:
                f.seek(chunk['start'])
                chunk_data = f.read(chunk['end'] - chunk['start'])
                if e['is_encrypted']: chunk_data = tool.xor_data(chunk_data)
                if e['zip'] != 0:
                    chunk_data = zlib.decompress(chunk_data)
                data.extend(chunk_data)
            return target_path, bytes(data)
        else:
            f.seek(e['offset'])
            read_size = e['zsize'] if e['zip'] != 0 else e['size']
            chunk_data = f.read(read_size)
            if e['is_encrypted']: chunk_data = tool.xor_data(chunk_data)
            if e['zip'] != 0:
                chunk_data = zlib.decompress(chunk_data)
            return target_path, chunk_data

def rebuild_v10_index(index_data, is_enc, new_assets, src_mp, target_mp):
    if is_enc: index_data = xor_bytes(index_data)
    br = pt.BinaryStream(index_data)
    
    mp_start = br.pos
    mount_point = br.read_string()
    mp_end = br.pos
    mp_bytes = index_data[mp_start:mp_end]
    
    num_entries = br.read_int32()
    entries_start = br.pos
    for _ in range(num_entries):
        br.pos += 36
        zip_val = br.read_int32()
        br.pos += 29
        if zip_val != 0:
            chunk_count = br.read_int32()
            br.pos += chunk_count * 16
        br.pos += 5
    entries_end = br.pos
    old_entries_bytes = index_data[entries_start:entries_end]
    
    global_entries = br.read_int64()
    dir_count = br.read_int64()
    
    dir_dict = {}
    dir_order = []
    for _ in range(dir_count):
        d_name = br.read_string()
        d_files = br.read_int64()
        files = []
        for _ in range(d_files):
            f_name = br.read_string()
            e_idx = br.read_int32()
            files.append((f_name, e_idx))
        dir_dict[d_name] = files
        dir_order.append(d_name)
        
    new_entries_bytes = bytearray()
    
    for rel_path_in_target, meta_bytes in new_assets:
        abs_path = target_mp + rel_path_in_target
        abs_path = abs_path.replace('//', '/')
        src_mp_clean = src_mp.replace('//', '/')
        if abs_path.startswith(src_mp_clean):
            rel_path_in_src = abs_path[len(src_mp_clean):]
        else:
            rel_path_in_src = abs_path
            
        idx = rel_path_in_src.rfind('/')
        if idx == -1:
            new_dir_name = ""
            new_file_name = rel_path_in_src
        else:
            new_dir_name = rel_path_in_src[:idx+1]
            new_file_name = rel_path_in_src[idx+1:]
            
        new_entries_bytes.extend(meta_bytes)
        target_entry_idx = num_entries
        num_entries += 1
        global_entries += 1
        if new_dir_name not in dir_dict:
            dir_dict[new_dir_name] = []
            dir_order.append(new_dir_name)
            dir_count += 1
        dir_dict[new_dir_name].append((new_file_name, target_entry_idx))
        
    new_index = bytearray()
    new_index.extend(mp_bytes)
    new_index.extend(struct.pack('<i', num_entries))
    new_index.extend(old_entries_bytes)
    new_index.extend(new_entries_bytes)
    new_index.extend(struct.pack('<q', global_entries))
    new_index.extend(struct.pack('<q', dir_count))
    
    for d_name in dir_order:
        new_index.extend(serialize_ue4_string(d_name))
        files = dir_dict[d_name]
        new_index.extend(struct.pack('<q', len(files)))
        for f_name, e_idx in files:
            new_index.extend(serialize_ue4_string(f_name))
            new_index.extend(struct.pack('<i', e_idx))
            
    if is_enc: return xor_bytes(new_index)
    return new_index

def compare_extracted_assets_debug(target_asset_pak, injected_pak, asset_base_name):
    print("\n  [🔍 深度Debug] === 自动解包与差异溯源测试 (Zip=3 压缩版) ===")
    
    orig_tool = pt.UE4PakEngine(target_asset_pak)
    orig_tool.parse()
    _, orig_uasset = extract_asset(orig_tool, target_asset_pak, ".uasset", asset_base_name)
    _, orig_uexp = extract_asset(orig_tool, target_asset_pak, ".uexp", asset_base_name)
    
    inj_tool = pt.UE4PakEngine(injected_pak)
    inj_tool.parse()
    _, inj_uasset = extract_asset(inj_tool, injected_pak, ".uasset", asset_base_name)
    _, inj_uexp = extract_asset(inj_tool, injected_pak, ".uexp", asset_base_name)
    
    if not orig_uasset or not inj_uasset:
        print("  [!] 提取失败，无法比对。")
        return

    print(f"  [🔍] 原版 uasset 解压后体积: {len(orig_uasset)} 字节")
    print(f"  [🔍] 注入版 uasset 解压后体积: {len(inj_uasset)} 字节")
    
    if orig_uasset == inj_uasset and orig_uexp == inj_uexp:
        print("  [✅ 终极确认] 从 Injected Pak 解包提取的资产数据，与官方原包解包数据 100% 完美一致！(Zip=3 压缩与解压闭环通过)")
    else:
        print("  [❌ 错误] 压缩/解压缩过程存在数据不一致！")

def add_asset_to_pak(src_pak, target_asset_pak, asset_base_name, out_pak):
    print(f"\n[+] 开始跨版本无缝移植 V10 资产 (Zip=3 压缩模式): {asset_base_name}")
    
    target_tool = pt.UE4PakEngine(target_asset_pak)
    if not target_tool.parse():
        return False
        
    uasset_path, uasset_data = extract_asset(target_tool, target_asset_pak, ".uasset", asset_base_name)
    uexp_path, uexp_data = extract_asset(target_tool, target_asset_pak, ".uexp", asset_base_name)
    
    if not uasset_data or not uexp_data:
        print("[!] 错误：找不到完整的资产文件！")
        return False
        
    src_tool = pt.UE4PakEngine(src_pak)
    if not src_tool.parse(): return False
        
    index_offset = src_tool.index_offset
    is_enc = src_tool.is_encrypted
    
    shutil.copy2(src_pak, out_pak)
    
    with open(out_pak, 'r+b') as f:
        f.seek(0, 2)
        file_size = f.tell()
        index_size = file_size - 45 - index_offset
        
        f.seek(index_offset)
        old_index_data = bytearray(f.read(index_size))
        
        # --- 尾部追加数据阶段 (Zip=3 分块压缩写入) ---
        f.seek(index_offset)
        
        def append_new_file_zip3(uncompressed_data, name_ext):
            file_offset = f.tell()
            # 获取构造好的 Meta 和 已经压缩好的数据体
            meta, compressed_payload = compress_and_create_fpakentry(file_offset, uncompressed_data)
            
            f.write(meta) 
            f.write(compressed_payload) 
            
            hash_hex = hashlib.sha1(uncompressed_data).digest().hex().upper()
            print(f"  [🔍 Debug] 写入 {name_ext} (Zlib) -> 偏移: 0x{file_offset:X} | 解压SHA1: {hash_hex} | 压缩率: {len(compressed_payload)}/{len(uncompressed_data)} 字节")
            return file_offset, meta, compressed_payload
            
        uasset_offset, uasset_meta, uasset_comp = append_new_file_zip3(uasset_data, "uasset")
        uexp_offset, uexp_meta, uexp_comp = append_new_file_zip3(uexp_data, "uexp")
        print("[*] 物理数据追加完成 (Zip=3 Zlib 分块压缩模式)。")
        
        # --- 重建 V10 Directory Map 阶段 ---
        new_assets_list = [
            (uasset_path, uasset_meta),
            (uexp_path, uexp_meta)
        ]
        
        new_index_bytes = rebuild_v10_index(
            old_index_data, is_enc, new_assets_list, 
            src_tool.mount_point, target_tool.mount_point
        )
        
        new_index_offset = f.tell()
        f.write(new_index_bytes)
        new_index_size = len(new_index_bytes)
        
        # --- 尾部指针重写阶段 (PakInfo) ---
        f.seek(file_size - 45)
        pak_info = bytearray(f.read(45))
        
        new_offset_enc = new_index_offset ^ OFFSET_KEY
        pak_info[37:45] = struct.pack('<Q', new_offset_enc)
        pak_info[29:37] = struct.pack('<q', new_index_size)
        
        index_hash = hashlib.sha1(new_index_bytes).digest()
        pak_info[9:29] = index_hash
        
        f.seek(new_index_offset + new_index_size)
        f.write(pak_info)
        f.truncate()
        
        # ================== 硬盘逐字节比对自检 ==================
        print("\n  [🔍 Debug] --- 终极二进制一致性自检 (Byte-for-byte Validation) ---")
        def verify_written_data(name_ext, offset, expected_meta, expected_comp_data):
            expected_full_chunk = expected_meta + expected_comp_data
            expected_len = len(expected_full_chunk)
            f.seek(offset)
            actual_full_chunk = f.read(expected_len)
            
            if expected_full_chunk == actual_full_chunk:
                print(f"  [✅ 成功] {name_ext} 压缩块硬盘写入 ({expected_len} 字节) 逐字节比对 100% 完美吻合！")
            else:
                print(f"  [❌ 失败] {name_ext} 压缩块硬盘写入数据损坏！")
                
        verify_written_data("uasset", uasset_offset, uasset_meta, uasset_comp)
        verify_written_data("uexp", uexp_offset, uexp_meta, uexp_comp)

    print(f"\n[+] 资产无缝注入成功！文件已生成: {out_pak}\n")
    return True

def main():
    print(WATERMARK)
    base_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "老6自动")
    
    print("请选择操作:")
    print("1. [常规] 解包 PAK")
    print("2. [常规] 重新打包 (原地覆盖)")
    print("3. [进阶] 跨版本无缝注入资产 (Add Asset - Zip=3版)")
    
    choice = input("输入序号: ").strip()
    
    if choice == "3":
        src_folder = os.path.join(base_path, "原版补丁PAK")
        target_folder = os.path.join(base_path, "范围资产OBB")
        
        if not os.path.exists(src_folder): os.makedirs(src_folder)
        if not os.path.exists(target_folder): os.makedirs(target_folder)
            
        src_files = [os.path.join(src_folder, f) for f in os.listdir(src_folder) if f.lower().endswith('.pak')]
        target_files = [os.path.join(target_folder, f) for f in os.listdir(target_folder) if f.lower().endswith(('.pak', '.obb'))]
        
        if not src_files or not target_files:
            print(f"\n[!] 目录为空，请补充基础包或提取池文件！")
            return

        try:
            print(f"\n[?] 请选择基础 PAK (被注入目标) - 位于 {src_folder}:")
            for i, f in enumerate(src_files): print(f"  {i+1}. {os.path.basename(f)}")
            src_idx = int(input("输入序号: ").strip()) - 1
            src_pak = src_files[src_idx]
            
            print(f"\n[?] 请选择提取池 PAK/OBB (新资产来源) - 位于 {target_folder}:")
            for i, f in enumerate(target_files): print(f"  {i+1}. {os.path.basename(f)}")
            target_idx = int(input("输入序号: ").strip()) - 1
            target_pak = target_files[target_idx]
            
            default_asset = "CH_Base_SK_PhysicsAsset"
            asset_name = input(f"\n请输入想要提取并注入的资产名 (直接回车默认: {default_asset}): ").strip() or default_asset
                
            file_name, ext = os.path.splitext(os.path.basename(src_pak))
            out_pak = os.path.join(base_path, f"{file_name}_Injected{ext}")
            
            if add_asset_to_pak(src_pak, target_pak, asset_name, out_pak):
                # 自动调用解包验证以确保 UAssetGUI 兼容性
                compare_extracted_assets_debug(target_pak, out_pak, asset_name)
                
        except (ValueError, IndexError):
            print("\n[-] 输入无效！程序退出。")
            return
            
    elif choice in ["1", "2"]:
        pak_path = input("请输入 PAK 路径: ").strip()
        work_dir = input("请输入工作目录(解包/打包存放处): ").strip()
        if not os.path.exists(pak_path): return
        tool = pt.UE4PakEngine(pak_path)
        if tool.parse():
            tool.extract(work_dir) if choice == "1" else tool.reimport(work_dir)

if __name__ == "__main__":
    main()