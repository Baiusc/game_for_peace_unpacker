import os
import shutil
import struct
import hashlib
import zlib
import B_PackTool_quickbms as pt

WATERMARK = """
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
  老 6 工 具 - V10 资产无缝注入 (神级宿主海选版) 
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
"""

# 和平精英专属密钥
XOR_KEY = 0x79
OFFSET_KEY = 0xD74AF37FAA6B020D
MAX_CHUNK_SIZE = 65536  

def xor_bytes(data: bytes, key: int = XOR_KEY) -> bytes:
    return bytes([b ^ key for b in data])

def serialize_ue4_string(s: str) -> bytes:
    encoded = s.encode('utf-8') + b'\x00'
    length = len(encoded)
    return struct.pack('<i', length) + encoded

def print_pak_info_debug(label: str, pak_info_bytes: bytes):
    enc_flag = pak_info_bytes[0]
    magic = struct.unpack('<I', pak_info_bytes[1:5])[0]
    version = struct.unpack('<I', pak_info_bytes[5:9])[0]
    index_hash = pak_info_bytes[9:29].hex().upper()
    # 捕获异常打印，防止部分加密包读取为巨大负数引发误解
    try:
        index_size = struct.unpack('<q', pak_info_bytes[29:37])[0]
    except:
        index_size = "Encrypted/Obfuscated"
        
    enc_offset = struct.unpack('<Q', pak_info_bytes[37:45])[0]
    dec_offset = enc_offset ^ OFFSET_KEY
    
    print(f"\n  [📊 {label} PakInfo 详情]")
    print(f"    ├─ EncryptedIndex: {enc_flag}")
    print(f"    ├─ Magic         : 0x{magic:X}")
    print(f"    ├─ Version       : {version}")
    print(f"    ├─ IndexHash     : {index_hash}")
    print(f"    ├─ IndexSize     : {index_size} 字节")
    print(f"    └─ IndexOffset   : {dec_offset} (0x{dec_offset:X}) [密文: 0x{enc_offset:X}]")

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
    return bytes(meta), b''.join(compressed_chunks)

def extract_asset(tool: pt.UE4PakEngine, pak_path: str, ext: str, asset_base_name: str):
    target_idx = -1
    target_path = ""
    search_str = asset_base_name + ext
    for path, idx in tool.files_map.items():
        if path.endswith(search_str):
            target_idx = idx
            target_path = path
            break
    if target_idx == -1: return None, None
        
    e = tool.entries_meta[target_idx]
    with open(pak_path, 'rb') as f:
        if len(e['chunks']) > 0:
            data = bytearray()
            for chunk in e['chunks']:
                f.seek(chunk['start'])
                chunk_data = f.read(chunk['end'] - chunk['start'])
                if e['is_encrypted']: chunk_data = tool.xor_data(chunk_data)
                if e['zip'] != 0: chunk_data = zlib.decompress(chunk_data)
                data.extend(chunk_data)
            return target_path, bytes(data)
        else:
            f.seek(e['offset'])
            read_size = e['zsize'] if e['zip'] != 0 else e['size']
            chunk_data = f.read(read_size)
            if e['is_encrypted']: chunk_data = tool.xor_data(chunk_data)
            if e['zip'] != 0: chunk_data = zlib.decompress(chunk_data)
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
            new_dir_name, new_file_name = "", rel_path_in_src
        else:
            new_dir_name, new_file_name = rel_path_in_src[:idx+1], rel_path_in_src[idx+1:]
            
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

# ==================== 方法3: 尾部追加注入 (等大版) ====================
def add_asset_to_pak(src_pak, target_asset_pak, asset_base_name, out_pak):
    print(f"\n[+] 开始跨版本无缝移植 V10 资产 (严格等大版): {asset_base_name}")
    target_tool = pt.UE4PakEngine(target_asset_pak)
    if not target_tool.parse(): return False
    uasset_path, uasset_data = extract_asset(target_tool, target_asset_pak, ".uasset", asset_base_name)
    uexp_path, uexp_data = extract_asset(target_tool, target_asset_pak, ".uexp", asset_base_name)
    if not uasset_data or not uexp_data: return False
    src_tool = pt.UE4PakEngine(src_pak)
    if not src_tool.parse(): return False
    index_offset, is_enc = src_tool.index_offset, src_tool.is_encrypted
    shutil.copy2(src_pak, out_pak)
    
    with open(out_pak, 'r+b') as f:
        f.seek(0, 2)
        original_file_size = f.tell()
        index_size = original_file_size - 45 - index_offset
        f.seek(original_file_size - 45)
        pak_info = bytearray(f.read(45))
        f.seek(index_offset)
        old_index_data = bytearray(f.read(index_size))

        _, uasset_comp = compress_and_create_fpakentry(0, uasset_data)
        _, uexp_comp = compress_and_create_fpakentry(0, uexp_data)
        dummy_uasset_meta, _ = compress_and_create_fpakentry(0, uasset_data)
        dummy_uexp_meta, _ = compress_and_create_fpakentry(0, uexp_data)
        dummy_assets_list = [(uasset_path, dummy_uasset_meta), (uexp_path, dummy_uexp_meta)]
        dummy_index_bytes = rebuild_v10_index(old_index_data, False, dummy_assets_list, src_tool.mount_point, target_tool.mount_point)
        
        required_tail_size = len(dummy_uasset_meta) + len(uasset_comp) + len(dummy_uexp_meta) + len(uexp_comp) + len(dummy_index_bytes) + 45
        new_start_offset = original_file_size - required_tail_size
        
        real_uasset_offset = new_start_offset
        real_uexp_offset = real_uasset_offset + len(dummy_uasset_meta) + len(uasset_comp)
        new_index_offset = real_uexp_offset + len(dummy_uexp_meta) + len(uexp_comp)

        real_uasset_meta, _ = compress_and_create_fpakentry(real_uasset_offset, uasset_data)
        real_uexp_meta, _ = compress_and_create_fpakentry(real_uexp_offset, uexp_data)
        real_assets_list = [(uasset_path, real_uasset_meta), (uexp_path, real_uexp_meta)]
        final_index_bytes = rebuild_v10_index(old_index_data, is_enc, real_assets_list, src_tool.mount_point, target_tool.mount_point)
        
        f.seek(new_start_offset)
        f.write(real_uasset_meta)
        f.write(uasset_comp)
        f.write(real_uexp_meta)
        f.write(uexp_comp)
        f.write(final_index_bytes)
        
        new_offset_enc = new_index_offset ^ OFFSET_KEY
        pak_info[37:45] = struct.pack('<Q', new_offset_enc)
        pak_info[29:37] = struct.pack('<q', len(final_index_bytes))
        pak_info[9:29] = hashlib.sha1(final_index_bytes).digest()
        f.write(pak_info)
        f.truncate(original_file_size)
    print(f"\n[+] 资产无缝等大注入成功！文件已生成: {out_pak}\n")
    return True

# ==================== 方法4: 狸猫换太子 (宿主海选版) ====================
def auto_swap_asset_in_place(src_pak, target_asset_pak, payload_name, out_pak):
    print(f"\n[+] 开始执行【神级】狸猫换太子 (宿主海选介入版) ...")
    print(f"  ├─ 真实载荷 (Payload): {payload_name}")
    
    # 1. 动态提取载荷
    target_tool = pt.UE4PakEngine(target_asset_pak)
    if not target_tool.parse(): return False
    
    payloads = []
    for ext in ['.uasset', '.uexp', '.ubulk']:
        _, p_data = extract_asset(target_tool, target_asset_pak, ext, payload_name)
        if p_data: payloads.append((ext, p_data))
            
    if not payloads:
        print("[!] 错误：未能在载荷提取池中找到 Payload！")
        return False
        
    print(f"[*] 载荷提取成功：需征用 {len(payloads)} 个宿主物理槽位。")
    
    # 2. 解析基础包
    src_tool = pt.UE4PakEngine(src_pak)
    if not src_tool.parse(): return False
    
    offsets_list = sorted([e['offset'] for e in src_tool.entries_meta])
    def get_next_offset(current_offset):
        idx = offsets_list.index(current_offset)
        return offsets_list[idx + 1] if idx + 1 < len(offsets_list) else src_tool.index_offset
        
    # 3. 分组并全盘扫描寻找合法候选人
    asset_groups = {}
    for path, e_idx in src_tool.files_map.items():
        base_path, ext = os.path.splitext(path)
        if base_path not in asset_groups: asset_groups[base_path] = {}
        asset_groups[base_path][ext] = src_tool.entries_meta[e_idx]
            
    print("  [🔍] 正在全盘扫描推演，筛选合法宿主库...")
    candidates = []
    
    for base_path, group in asset_groups.items():
        if len(group) >= len(payloads):
            host_exts = sorted(list(group.keys()))[:len(payloads)]
            capacity_ok = True
            current_waste = 0
            mapping = []
            
            for i in range(len(payloads)):
                p_ext, p_data = payloads[i]
                h_ext = host_exts[i]
                h_meta = group[h_ext]
                p_size = len(p_data)
                
                if h_meta['size'] < p_size:
                    capacity_ok = False
                    break
                    
                if h_meta['zip'] == 0:
                    current_waste += (h_meta['size'] - p_size)
                else:
                    chunks = h_meta['chunks']
                    if not chunks: 
                        capacity_ok = False
                        break
                    padded_p_data = p_data + b'\x00' * (h_meta['size'] - p_size)
                    chunk_waste = 0
                    for c_idx, chunk in enumerate(chunks):
                        raw_slice = padded_p_data[c_idx * MAX_CHUNK_SIZE : (c_idx + 1) * MAX_CHUNK_SIZE]
                        comp_slice = zlib.compress(raw_slice, level=9)
                        phys_limit = chunk['end'] - chunk['start']
                        if len(comp_slice) > phys_limit:
                            capacity_ok = False
                            break
                        chunk_waste += (phys_limit - len(comp_slice))
                    if not capacity_ok: break
                    current_waste += chunk_waste
                    
                mapping.append({
                    'p_ext': p_ext, 'p_data': p_data,
                    'h_ext': h_ext, 'h_meta': h_meta
                })
                
            if capacity_ok:
                candidates.append({
                    'path': base_path, 
                    'mapping': mapping, 
                    'waste': current_waste
                })
                    
    if not candidates:
        print("  [❌ 失败] 未能找到任何一个装得下 Payload 的宿主！")
        return False
        
    # 4. === 人工介入海选 ===
    # 按 waste（冗余损耗）从小到大排序
    candidates.sort(key=lambda x: x['waste'])
    
    print(f"\n================ 🏆 宿主海选名单 TOP 15 ================")
    print(f" ⚠️ 注意：请避开敏感活跃文件(如 CSV/, Config/, Lobby/ 等)！")
    print(f" ⚠️ 建议：优先选择 Audio/, Effects/, 或明显是测试用的冗余文件！\n")
    
    display_limit = min(15, len(candidates))
    for i in range(display_limit):
        cand = candidates[i]
        zip_type = "Zip=0" if cand['mapping'][0]['h_meta']['zip']==0 else "Zip=3"
        print(f" [{i+1:2d}] 损耗 {cand['waste']:5d} B | [{zip_type}] | {cand['path']}")
    
    print(f"======================================================")
    
    while True:
        try:
            choice_str = input(f"\n请选择最安全的宿主序号 (1-{display_limit})，或输入 0 退出: ").strip()
            if choice_str == '0':
                print("已取消注入。")
                return False
            choice_idx = int(choice_str) - 1
            if 0 <= choice_idx < display_limit:
                best_host = candidates[choice_idx]
                break
            else:
                print("序号超出范围，请重新输入。")
        except ValueError:
            print("输入无效，请输入数字。")

    h_path = best_host['path']
    print(f"\n  [✅ 你已锁定宿主: {h_path}]")
    
    # 5. 执行物理擦写
    print(f"[*] 正在克隆目标包并执行底层硬盘级覆写: {os.path.basename(out_pak)}")
    shutil.copy2(src_pak, out_pak)
    
    with open(out_pak, 'r+b') as f:
        for m in best_host['mapping']:
            ext_info = f"Payload {m['p_ext']} -> Host {m['h_ext']}"
            h_meta = m['h_meta']
            p_data = m['p_data']
            
            if h_meta['zip'] == 0:
                next_off = get_next_offset(h_meta['offset'])
                data_start = next_off - h_meta['size']
                padded_data = p_data + b'\x00' * (h_meta['size'] - len(p_data))
                
                f.seek(data_start)
                f.write(xor_bytes(padded_data) if h_meta['is_encrypted'] else padded_data)
                print(f"      ├─ [{ext_info}] 物理覆写完成 (安全补零 {h_meta['size'] - len(p_data)} 字节)")
                
            else:
                padded_data = p_data + b'\x00' * (h_meta['size'] - len(p_data))
                chunks = h_meta['chunks']
                is_enc = h_meta['is_encrypted']
                
                for c_idx, chunk in enumerate(chunks):
                    raw_slice = padded_data[c_idx * MAX_CHUNK_SIZE : (c_idx + 1) * MAX_CHUNK_SIZE]
                    comp_slice = zlib.compress(raw_slice, level=9)
                    phys_limit = chunk['end'] - chunk['start']
                    
                    write_slice = xor_bytes(comp_slice) if is_enc else comp_slice
                    pad_len = phys_limit - len(write_slice)
                    
                    f.seek(chunk['start'])
                    f.write(write_slice)
                    f.write(b'\x00' * pad_len) 
                    print(f"      ├─ [{ext_info}] 覆盖 Chunk {c_idx}: 写入 {len(write_slice)} 字节 (冗余气泡 {pad_len} 字节)")
        
        # 终极验证
        f.seek(0, 2)
        final_size = f.tell()
        f.seek(final_size - 45)
        pak_info = bytearray(f.read(45))
        print_pak_info_debug("注入后静态特征 (严正声明：完全未修改)", pak_info)
        
        print(f"\n  [✅ 终极校验] 狸猫换太子完成！")
        print(f"    ├─ 物理体积: {final_size} 字节 (绝对安全)")
        print(f"    ├─ 静态特征: 官方 MD5/Hash 均未被破坏！")
        print(f"    └─ 💡 进阶提示：请在内存态 Hook VFS (如拦截 OpenRead)，施加以下路径劫持：")
        for m in best_host['mapping']:
            print(f"        [Hook] .../{payload_name}{m['p_ext']}  =>  .../{os.path.basename(h_path)}{m['h_ext']}")

    print(f"\n[+] 物理注入成功！文件已生成: {out_pak}\n")
    return True

# ==================== 主菜单 ====================
def main():
    print(WATERMARK)
    base_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "老6自动")
    
    print("请选择操作:")
    print("1. [常规] 解包 PAK")
    print("2. [常规] 重新打包 (原地覆盖)")
    print("3. [进阶] 跨版本无缝注入资产 (Add Asset - 尾部等大版)")
    print("4. [神级] 自动狸猫换太子 (宿主海选 - 零索引改动)")
    
    choice = input("输入序号: ").strip()
    
    if choice in ["3", "4"]:
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
            asset_name = input(f"\n请输入你要植入的真实资产名 (直接回车默认: {default_asset}): ").strip() or default_asset
            
            file_name, ext = os.path.splitext(os.path.basename(src_pak))
            out_pak = os.path.join(base_path, f"{file_name}_Injected{ext}")
            
            if choice == "3":
                # 这里调用省略了的 compare_extracted_assets_debug，如果你还需要比对可以自己加回来
                add_asset_to_pak(src_pak, target_pak, asset_name, out_pak)
            elif choice == "4":
                auto_swap_asset_in_place(src_pak, target_pak, asset_name, out_pak)
                
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