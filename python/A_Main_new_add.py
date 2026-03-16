import os
import shutil
import struct
import hashlib
import zlib
import B_PackTool_quickbms as pt

WATERMARK = """
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
  老 6 工 具 - V10 资产无缝注入 (Zip=3 终极修复版) 
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
"""

# 和平精英专属密钥
XOR_KEY = 0x79
OFFSET_KEY = 0xD74AF37FAA6B020D
MAX_CHUNK_SIZE = 65536  # UE4 官方标准压缩块大小 64KB

def xor_bytes(data: bytes, key: int = XOR_KEY) -> bytes:
    return bytes([b ^ key for b in data])

def serialize_ue4_string(s: str) -> bytes:
    encoded = s.encode('utf-8') + b'\x00'
    length = len(encoded)
    return struct.pack('<i', length) + encoded

def compress_and_create_fpakentry(file_offset: int, uncompressed_data: bytes) -> tuple:
    uncompressed_size = len(uncompressed_data)
    compressed_chunks = []
    
    for i in range(0, uncompressed_size, MAX_CHUNK_SIZE):
        raw_chunk = uncompressed_data[i : i + MAX_CHUNK_SIZE]
        comp_chunk = zlib.compress(raw_chunk, level=9)
        compressed_chunks.append(comp_chunk)
        
    chunk_count = len(compressed_chunks)
    meta_size = 69 + 4 + (16 * chunk_count) + 5
    payload_start_offset = file_offset + meta_size
    
    chunk_offsets = []
    current_offset = payload_start_offset
    zsize = 0
    for comp_chunk in compressed_chunks:
        c_len = len(comp_chunk)
        chunk_offsets.append((current_offset, current_offset + c_len))
        current_offset += c_len
        zsize += c_len
        
    data_hash = hashlib.sha1(uncompressed_data).digest()

    meta = bytearray()
    meta.extend(data_hash)                     
    meta.extend(struct.pack('<q', file_offset))
    meta.extend(struct.pack('<q', uncompressed_size)) 
    meta.extend(struct.pack('<i', 3))          
    meta.extend(struct.pack('<q', zsize))      
    meta.extend(b'\x00' * 21)                  
    meta.extend(struct.pack('<i', chunk_count))
    for start_off, end_off in chunk_offsets:
        meta.extend(struct.pack('<q', start_off)) 
        meta.extend(struct.pack('<q', end_off))   
    meta.extend(struct.pack('<i', MAX_CHUNK_SIZE)) 
    meta.extend(b'\x00')                           

    compressed_payload = b''.join(compressed_chunks)
    return bytes(meta), compressed_payload

def extract_asset(tool: pt.UE4PakEngine, pak_path: str, ext: str, asset_base_name: str):
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

def compare_extracted_assets_debug(src_pak, target_asset_pak, injected_pak, asset_base_name):
    print("\n  [🔍 深度Debug] === 自动解包与差异溯源测试 (Zip=3 三方比对版) ===")
    
    # 1. 尝试从原版基础包 (src_pak) 中提取，看之前到底有没有这个文件
    print(f"  [🔍] 正在解析原版基础包: {os.path.basename(src_pak)}...")
    src_tool = pt.UE4PakEngine(src_pak)
    src_tool.parse()
    _, src_uasset = extract_asset(src_tool, src_pak, ".uasset", asset_base_name)
    _, src_uexp = extract_asset(src_tool, src_pak, ".uexp", asset_base_name)
    
    if not src_uasset:
        print(f"  [🔍] 注入前状态: 原包内未找到 '{asset_base_name}' (这是一个纯新增的注入跨界资产！)")
    else:
        print(f"  [🔍] 注入前状态: 原包内存在旧资产，uasset 解压后体积: {len(src_uasset)} 字节")

    # 2. 从目标资产池 (target_asset_pak，即范围伤害OBB) 提取，作为【标准答案】
    target_tool = pt.UE4PakEngine(target_asset_pak)
    target_tool.parse()
    _, target_uasset = extract_asset(target_tool, target_asset_pak, ".uasset", asset_base_name)
    _, target_uexp = extract_asset(target_tool, target_asset_pak, ".uexp", asset_base_name)

    # 3. 从我们生成的注入包 (injected_pak) 中提取，验证注入结果
    print(f"  [🔍] 正在解析注入生成包: {os.path.basename(injected_pak)}...")
    inj_tool = pt.UE4PakEngine(injected_pak)
    inj_tool.parse()
    _, inj_uasset = extract_asset(inj_tool, injected_pak, ".uasset", asset_base_name)
    _, inj_uexp = extract_asset(inj_tool, injected_pak, ".uexp", asset_base_name)
    
    if not target_uasset or not inj_uasset:
        print("  [!] 提取失败，无法进行比对验证。")
        return

    print(f"  [🔍] 注入后状态: 新包内 uasset 解压后体积: {len(inj_uasset)} 字节")
    
    # --- 终极交叉对比 ---
    if target_uasset == inj_uasset and target_uexp == inj_uexp:
        print("  [✅ 终极确认] 从 Injected Pak 解包出的新资产，与高版本提取池的【标准答案】 100% 完美一致！(Zip=3 压缩闭环通过)")
        if src_uasset:
            if src_uasset != inj_uasset:
                print("  [✅ 覆盖确认] 成功覆盖了基础包中原有的旧资产，替换生效！")
            else:
                print("  [⚠️ 提示] 注入的资产与基础包中原有的资产内容完全相同，体积未发生实质性变化。")
    else:
        print("  [❌ 错误] 压缩/解压缩过程存在数据不一致！")

def add_asset_to_pak(src_pak, target_asset_pak, asset_base_name, out_pak):
    print(f"\n[+] 开始跨版本无缝移植 V10 资产 (Zip=3 压缩模式): {asset_base_name}")
    
    target_tool = pt.UE4PakEngine(target_asset_pak)
    if not target_tool.parse(): return False
        
    uasset_path, uasset_data = extract_asset(target_tool, target_asset_pak, ".uasset", asset_base_name)
    uexp_path, uexp_data = extract_asset(target_tool, target_asset_pak, ".uexp", asset_base_name)
    if not uasset_data or not uexp_data: return False
        
    src_tool = pt.UE4PakEngine(src_pak)
    if not src_tool.parse(): return False
        
    index_offset = src_tool.index_offset
    is_enc = src_tool.is_encrypted
    
    shutil.copy2(src_pak, out_pak)
    
    with open(out_pak, 'r+b') as f:
        f.seek(0, 2)
        file_size = f.tell()
        index_size = file_size - 45 - index_offset
        
        # ================= 救命的修复：提前读取并解析 PakInfo =================
        f.seek(file_size - 45)
        pak_info = bytearray(f.read(45))
        
        # 解析原始的 PakInfo，为了验证我们没有弄坏它
        orig_enc_flag = pak_info[0]
        orig_magic = struct.unpack('<I', pak_info[1:5])[0]
        orig_version = struct.unpack('<I', pak_info[5:9])[0]
        
        print(f"\n  [🔍 尾部防覆盖保护] 成功读取原始 PakInfo。")
        print(f"      -> 原始 Magic: 0x{orig_magic:X} (游戏引擎识别的核心标志)")
        print(f"      -> 原始 Version: {orig_version} (确保不会变成乱码)")
        # ======================================================================

        # 读取旧的索引区数据
        f.seek(index_offset)
        old_index_data = bytearray(f.read(index_size))
        
        # --- 尾部追加数据阶段 ---
        f.seek(index_offset)
        def append_new_file_zip3(uncompressed_data, name_ext):
            file_offset = f.tell()
            meta, compressed_payload = compress_and_create_fpakentry(file_offset, uncompressed_data)
            f.write(meta) 
            f.write(compressed_payload) 
            return file_offset, meta, compressed_payload
            
        uasset_offset, uasset_meta, uasset_comp = append_new_file_zip3(uasset_data, "uasset")
        uexp_offset, uexp_meta, uexp_comp = append_new_file_zip3(uexp_data, "uexp")
        
        # --- 重建 V10 Directory Map 阶段 ---
        new_assets_list = [(uasset_path, uasset_meta), (uexp_path, uexp_meta)]
        new_index_bytes = rebuild_v10_index(old_index_data, is_enc, new_assets_list, src_tool.mount_point, target_tool.mount_point)
        
        new_index_offset = f.tell()
        f.write(new_index_bytes)
        new_index_size = len(new_index_bytes)
        
        # --- 尾部指针重写阶段 (PakInfo) ---
        # 此时不再去硬盘读了！直接使用内存中受保护的、携带正确 Magic 和 Version 的 pak_info
        
        new_offset_enc = new_index_offset ^ OFFSET_KEY
        pak_info[37:45] = struct.pack('<Q', new_offset_enc)
        pak_info[29:37] = struct.pack('<q', new_index_size)
        index_hash = hashlib.sha1(new_index_bytes).digest()
        pak_info[9:29] = index_hash
        
        # 封口并截断 (多余的数据会被物理删除)
        final_file_size = new_index_offset + new_index_size + 45
        f.seek(new_index_offset + new_index_size)
        f.write(pak_info)
        f.truncate()
        
        # ================= 封口自测：验证 Version 依然完好 =================
        f.seek(-45, 2)
        verify_pak_info = f.read(45)
        verify_version = struct.unpack('<I', verify_pak_info[5:9])[0]
        if verify_version == orig_version:
            print(f"  [✅ 成功] 物理尾部封口完成！(Version 保持为: {verify_version})，游戏引擎可正常识别！\n")
        else:
            print(f"  [❌ 致命错误] 尾部被污染！当前 Version 为: {verify_version}\n")
        # ===================================================================

    print(f"[+] 资产无缝注入成功！文件已生成: {out_pak}\n")
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
                # 传入 src_pak (原包), target_pak (标准答案包), out_pak (注入包) 进行三方交叉比对
                compare_extracted_assets_debug(src_pak, target_pak, out_pak, asset_name)
                
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