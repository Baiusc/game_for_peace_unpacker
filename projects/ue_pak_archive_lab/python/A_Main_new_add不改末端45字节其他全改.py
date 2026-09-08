import os
import shutil
import struct
import hashlib
import zlib
import B_PackTool_quickbms as pt

WATERMARK = """
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
  老 6 工 具 - 终极原生注魂版 (TOCTOU 最终形态)
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

def patch_index_entry_inplace(index_data, entry_start, p_data, comp_chunks, is_zip):
    p_size = len(p_data)
    index_data[entry_start : entry_start + 20] = hashlib.sha1(p_data).digest()
    struct.pack_into('<q', index_data, entry_start + 28, p_size)
    
    if is_zip == 0:
        struct.pack_into('<q', index_data, entry_start + 40, p_size)
    else:
        total_zsize = sum(len(c) for c in comp_chunks)
        struct.pack_into('<q', index_data, entry_start + 40, total_zsize)
        chunk_array_start = entry_start + 73
        for i, chunk_info in enumerate(comp_chunks):
            c_start = chunk_info['start']
            c_end = chunk_info['end']
            struct.pack_into('<q', index_data, chunk_array_start + i * 16, c_start)
            struct.pack_into('<q', index_data, chunk_array_start + i * 16 + 8, c_end)

def get_next_offset_trick(entries_meta, current_offset, index_offset):
    offsets = sorted([e['offset'] for e in entries_meta])
    idx = offsets.index(current_offset)
    return offsets[idx + 1] if idx + 1 < len(offsets) else index_offset

def calc_str_delta(old_str, new_str):
    return len(serialize_ue4_string(new_str)) - len(serialize_ue4_string(old_str))

def safe_replace_bin(data_bytes, old_str, new_str):
    old_bin = serialize_ue4_string(old_str)
    new_bin = serialize_ue4_string(new_str)
    count = data_bytes.count(old_bin)
    if count == 0:
        print(f"        [!] 警告: 未找到目标字符串序列 '{old_str}'")
    return data_bytes.replace(old_bin, new_bin, 1)

# ==================== 方法6: 原生路径注魂 (字串碰撞平衡法则) ====================
def auto_swap_asset_native_path(src_pak, target_asset_pak, payload_name, out_pak):
    print(f"\n[+] 开始执行【终极】原生路径注魂 (字串碰撞平衡法则) ...")
    
    target_tool = pt.UE4PakEngine(target_asset_pak)
    if not target_tool.parse(): return False
    
    payloads = []
    for ext in ['.uasset', '.uexp', '.ubulk']:
        p_full_path, p_data = extract_asset(target_tool, target_asset_pak, ext, payload_name)
        if p_data: payloads.append({'ext': ext, 'data': p_data, 'path': p_full_path})
            
    if not payloads:
        print("[!] 错误：未能在载荷提取池中找到 Payload！")
        return False
        
    src_tool = pt.UE4PakEngine(src_pak)
    if not src_tool.parse(): return False
    
    with open(src_pak, 'rb') as f:
        f.seek(0, 2)
        total_size = f.tell()
        f.seek(total_size - 45)
        enc_offset = struct.unpack('<Q', f.read(45)[37:45])[0]
        index_offset = enc_offset ^ OFFSET_KEY
        real_index_size = total_size - 45 - index_offset
        f.seek(index_offset)
        raw_index_data = bytearray(f.read(real_index_size))
        
    work_index_data = bytearray(xor_bytes(raw_index_data)) if src_tool.is_encrypted else raw_index_data[:]
        
    br = pt.BinaryStream(work_index_data)
    br.read_string()
    num_entries = br.read_int32()
    
    entry_byte_offsets = [] 
    for _ in range(num_entries):
        start_pos = br.pos
        br.pos += 36
        zip_val = br.read_int32()
        br.pos += 29
        if zip_val != 0:
            chunk_count = br.read_int32()
            br.pos += chunk_count * 16
        br.pos += 5
        entry_byte_offsets.append({'start_pos': start_pos})

    # 🔑 修复点 1：不再进行危险的路径长度切片，原汁原味保留官方解析出的字符串
    asset_groups = {}
    all_dirs = set()
    for path, e_idx in src_tool.files_map.items():
        dir_name = os.path.dirname(path) + '/'
        all_dirs.add(dir_name)
        
        base_path, ext = os.path.splitext(path)
        if base_path not in asset_groups: asset_groups[base_path] = {}
        asset_groups[base_path][ext] = {
            'meta': src_tool.entries_meta[e_idx],
            'entry_start': entry_byte_offsets[e_idx]['start_pos'],
            'full_path': path
        }
            
    print("  [🔍] 正在海选物理宿主 (需提供足够物理空间)...")
    candidates = []
    
    for base_path, group in asset_groups.items():
        if len(group) >= len(payloads):
            host_exts = sorted(list(group.keys()))[:len(payloads)]
            capacity_ok = True
            current_waste = 0
            mapping = []
            
            for i in range(len(payloads)):
                p_ext = payloads[i]['ext']
                p_data = payloads[i]['data']
                h_ext = host_exts[i]
                h_info = group[h_ext]
                h_meta = h_info['meta']
                p_size = len(p_data)
                
                req_chunks = (p_size + MAX_CHUNK_SIZE - 1) // MAX_CHUNK_SIZE if p_size > 0 else 1
                
                if h_meta['zip'] == 0:
                    if h_meta['size'] < p_size: capacity_ok = False; break
                    current_waste += (h_meta['size'] - p_size)
                    mapping.append({'p_ext': p_ext, 'p_data': p_data, 'h_ext': h_ext, 'h_info': h_info})
                else:
                    h_chunks = h_meta['chunks']
                    if len(h_chunks) < req_chunks: capacity_ok = False; break 
                    
                    comp_chunks_plan = []
                    total_phys_needed = 0
                    for c_idx in range(req_chunks):
                        raw_slice = p_data[c_idx * MAX_CHUNK_SIZE : (c_idx + 1) * MAX_CHUNK_SIZE]
                        comp_slice = zlib.compress(raw_slice, level=9)
                        total_phys_needed += len(comp_slice)
                        comp_chunks_plan.append(comp_slice)
                        
                    host_phys_avail = sum(c['end'] - c['start'] for c in h_chunks)
                    if host_phys_avail < total_phys_needed: capacity_ok = False; break
                    
                    current_waste += (host_phys_avail - total_phys_needed)
                    mapping.append({
                        'p_ext': p_ext, 'p_data': p_data, 'h_ext': h_ext, 
                        'h_info': h_info, 'comp_plan': comp_chunks_plan
                    })
                
            if capacity_ok:
                candidates.append({'path': base_path, 'mapping': mapping, 'waste': current_waste})
                    
    if not candidates:
        return False
        
    candidates.sort(key=lambda x: x['waste'])
    print(f"\n================ 🏆 物理宿主海选名单 TOP 10 ================")
    display_limit = min(10, len(candidates))
    for i in range(display_limit):
        cand = candidates[i]
        print(f" [{i+1:2d}] 损耗 {cand['waste']:5d} B | {cand['path']}")
    
    while True:
        try:
            choice_str = input(f"请选择物理宿主序号 (1-{display_limit}): ").strip()
            choice_idx = int(choice_str) - 1
            if 0 <= choice_idx < display_limit:
                best_host = candidates[choice_idx]
                break
        except ValueError: pass

    # --- 🎭 核心路径偷换逻辑 ---
    # 🔑 修复点 2：直接使用原始完整路径获取基准目录名
    p_full = payloads[0]['path']
    p_dir = os.path.dirname(p_full) + '/'
    p_name = os.path.basename(p_full).split('.')[0]
    
    h_full = best_host['mapping'][0]['h_info']['full_path']
    h_dir = os.path.dirname(h_full) + '/'
    h_name = os.path.basename(h_full).split('.')[0]
    
    # --- 🛡️ 预探雷机制：确保目标字符串真的在二进制流中 ---
    if work_index_data.count(serialize_ue4_string(h_dir)) == 0:
        print(f"\n  [❌ 致命错误] 无法在底层二进制流中定位宿主目录: '{h_dir}'")
        print("  可能是官方序列化格式异常，放弃本次注魂以保护文件。")
        return False

    delta_dir = calc_str_delta(h_dir, p_dir)
    delta_files = sum(calc_str_delta(h_name + m['h_ext'], p_name + m['p_ext']) for m in best_host['mapping'])
    net_delta = delta_dir + delta_files
    
    print(f"\n  [⚖️ 空间平衡计算]")
    print(f"    ├─ 移除旧目录: -{len(h_dir)} | 注入新目录: +{len(p_dir)} (差额 {delta_dir})")
    print(f"    ├─ 移除旧文件: -{len(h_name)} | 注入新文件: +{len(p_name)} (差额 {delta_files})")
    print(f"    └─ 净空间收支 (Net Delta): {net_delta} 字节")
    
    v_dir_old = ""
    v_dir_new = ""
    
    if net_delta != 0:
        print(f"\n================ 🔪 路径献祭名单 (需精准平衡 {-net_delta} 字节) ================")
        victim_candidates = []
        for v in all_dirs:
            if v == h_dir or v == p_dir: continue
            if any(x in v.lower() for x in ['csv/', 'config/', 'lobby/', 'character/']): continue
            
            # 必须确保受害者字符串真的在二进制流中存在！
            if work_index_data.count(serialize_ue4_string(v)) == 0: continue
            
            if net_delta > 0: 
                base_v = v[:-1]
                if len(base_v) > net_delta + 5:
                    victim_candidates.append(v)
            elif net_delta < 0: 
                victim_candidates.append(v)
                
        victim_candidates.sort(key=lambda x: 'audio' in x.lower() or 'effect' in x.lower() or 'test' in x.lower(), reverse=True)
        v_limit = min(15, len(victim_candidates))
        
        for i in range(v_limit):
            v = victim_candidates[i]
            if net_delta > 0:
                preview = v[:-net_delta - 1] + '/'
            else:
                preview = v[:-1] + "_" * abs(net_delta) + '/'
            print(f" [{i+1:2d}] {v}  =>  变成: {preview}")
            
        while True:
            try:
                v_choice = input(f"请选择要斩首的冷门目录 (1-{v_limit}): ").strip()
                v_idx = int(v_choice) - 1
                if 0 <= v_idx < v_limit:
                    v_dir_old = victim_candidates[v_idx]
                    if net_delta > 0:
                        v_dir_new = v_dir_old[:-net_delta - 1] + '/'
                    else:
                        v_dir_new = v_dir_old[:-1] + "_" * abs(net_delta) + '/'
                    break
            except ValueError: pass

    print(f"\n  [*] 正在执行 Index 字符串二进制热替换...")
    work_index_data = safe_replace_bin(work_index_data, h_dir, p_dir)
    for m in best_host['mapping']:
        work_index_data = safe_replace_bin(work_index_data, h_name + m['h_ext'], p_name + m['p_ext'])
    if net_delta != 0:
        work_index_data = safe_replace_bin(work_index_data, v_dir_old, v_dir_new)
        
    if len(work_index_data) != real_index_size:
        print(f"  [❌ 致命错误] 空间平衡失败！原本 {real_index_size}，替换后 {len(work_index_data)}")
        return False
    print("  [✅] 空间完美守恒！总 IndexSize 未发生 1 Byte 改变。")

    print(f"[*] 正在克隆目标包并执行底层擦写: {os.path.basename(out_pak)}")
    shutil.copy2(src_pak, out_pak)
    
    with open(out_pak, 'r+b') as f:
        f.seek(0, 2)
        total_size = f.tell()
        f.seek(total_size - 45)
        pak_info_backup = bytearray(f.read(45))
        
        for m in best_host['mapping']:
            h_meta = m['h_info']['meta']
            entry_start = m['h_info']['entry_start']
            p_data = m['p_data']
            
            if h_meta['zip'] == 0:
                data_start = get_next_offset_trick(src_tool.entries_meta, h_meta['offset'], index_offset) - h_meta['size']
                padded_data = p_data + b'\x00' * (h_meta['size'] - len(p_data))
                f.seek(data_start)
                f.write(xor_bytes(padded_data) if h_meta['is_encrypted'] else padded_data)
                patch_index_entry_inplace(work_index_data, entry_start, p_data, [], 0)
            else:
                is_enc = h_meta['is_encrypted']
                comp_plan = m['comp_plan']
                h_chunks = h_meta['chunks']
                current_write_offset = h_chunks[0]['start']
                final_chunk_records = []
                
                f.seek(current_write_offset)
                for c_idx, comp_slice in enumerate(comp_plan):
                    write_slice = xor_bytes(comp_slice) if is_enc else comp_slice
                    c_len = len(write_slice)
                    f.write(write_slice)
                    final_chunk_records.append({'start': current_write_offset, 'end': current_write_offset + c_len})
                    current_write_offset += c_len
                    
                total_phys_avail = sum(c['end'] - c['start'] for c in h_chunks)
                total_phys_used = sum(c['end'] - c['start'] for c in final_chunk_records)
                f.write(b'\x00' * (total_phys_avail - total_phys_used))
                for _ in range(len(h_chunks) - len(comp_plan)):
                    final_chunk_records.append({'start': current_write_offset, 'end': current_write_offset})
                
                patch_index_entry_inplace(work_index_data, entry_start, p_data, final_chunk_records, 3)
        
        f.seek(index_offset)
        f.write(xor_bytes(work_index_data) if src_tool.is_encrypted else work_index_data)
        
        f.write(pak_info_backup)
        f.truncate(total_size) 
        
        print(f"\n  [✅ 终极胜利] 原生路径注魂完成！")
        print(f"    ├─ 物理实体: 已被变态骨骼霸占。")
        print(f"    ├─ 内部索引: 已改名为 {p_name} 且属性完美同步。")
        print(f"    ├─ 空间魔法: 通过献祭无辜目录，维持了 Size 绝对守恒。")
        print(f"    └─ 💡 拿去热拔插吧！游戏将原生加载该骨骼，你连外挂 Hook 都不需要了！")

    print(f"\n[+] TOCTOU 终极原生包已生成: {out_pak}\n")
    return True


# ==================== 主菜单 ====================
def main():
    print(WATERMARK)
    base_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "老6自动")
    
    print("请选择操作:")
    print("1. [常规] 解包 PAK")
    print("2. [常规] 重新打包 (原地覆盖)")
    print("3. [进阶] 跨版本无缝注入资产 (Add Asset - 尾部等大版)")
    print("4. [神级] 自动狸猫换太子 (索引属性内部同步 - 专供热拔插)")
    print("5. [研究] 破坏索引实体哈希 (测验 IndexHash 强校验)")
    print("6. [终极] 原生路径注魂 (字串碰撞平衡法则 - 无需内存Hook)")
    
    choice = input("输入序号: ").strip()
    
    if choice in ["3", "4", "5", "6"]:
        src_folder = os.path.join(base_path, "原版补丁PAK")
        target_folder = os.path.join(base_path, "范围资产OBB")
        if not os.path.exists(src_folder): os.makedirs(src_folder)
        if not os.path.exists(target_folder): os.makedirs(target_folder)
            
        src_files = [os.path.join(src_folder, f) for f in os.listdir(src_folder) if f.lower().endswith('.pak')]
        target_files = [os.path.join(target_folder, f) for f in os.listdir(target_folder) if f.lower().endswith(('.pak', '.obb'))]
        
        if not src_files or (choice != "5" and not target_files):
            print(f"\n[!] 目录为空！请检查基础包 (若选3/4/6还需检查提取池)！")
            return

        try:
            print(f"\n[?] 请选择基础 PAK (实验目标) - 位于 {src_folder}:")
            for i, f in enumerate(src_files): print(f"  {i+1}. {os.path.basename(f)}")
            src_idx = int(input("输入序号: ").strip()) - 1
            src_pak = src_files[src_idx]
            
            file_name, ext = os.path.splitext(os.path.basename(src_pak))
            out_pak = os.path.join(base_path, f"{file_name}_Injected{ext}")
            
            if choice == "5":
                # 省略了前面的代码，确保你把之前的功能都保留着
                pass
            else:
                print(f"\n[?] 请选择提取池 PAK/OBB (新资产来源) - 位于 {target_folder}:")
                for i, f in enumerate(target_files): print(f"  {i+1}. {os.path.basename(f)}")
                target_idx = int(input("输入序号: ").strip()) - 1
                target_pak = target_files[target_idx]
                
                default_asset = "CH_Base_SK_PhysicsAsset"
                asset_name = input(f"\n请输入你要植入的真实资产名 (直接回车默认: {default_asset}): ").strip() or default_asset
                
                if choice == "6":
                    auto_swap_asset_native_path(src_pak, target_pak, asset_name, out_pak)
                # elif choice == 4 ...
                
        except (ValueError, IndexError):
            print("\n[-] 输入无效！程序退出。")
            return
            
    elif choice in ["1", "2"]:
        # ...
        pass

if __name__ == "__main__":
    main()