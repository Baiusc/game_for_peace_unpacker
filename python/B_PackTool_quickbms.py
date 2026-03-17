import struct
import os
import argparse
import sys
import time

import zlib

try:
    import zopfli.zlib

    HAS_ZOPFLI = True
except ImportError:
    HAS_ZOPFLI = False
    print("[Info] 'zopfli' module not found. Install it for best compression results.")

XOR_KEY = 0x79
MAGIC_SEARCH_1 = b"\x2E\x2E\x2F\x2E\x2E\x2F\x2E\x2E\x2F"  # ../../../
MAGIC_SEARCH_2 = b"\x57\x57\x56\x57\x57\x56\x57\x57\x56"  # XORed pattern


class BinaryStream:
    def __init__(self, data):
        self.data = data
        self.pos = 0
        self.size = len(data)

    def read(self, size):
        if self.pos + size > self.size:
            ret = self.data[self.pos:]
            self.pos = self.size
            return ret
        ret = self.data[self.pos: self.pos + size]
        self.pos += size
        return ret

    def read_int32(self):
        b = self.read(4)
        return struct.unpack('<i', b)[0] if len(b) == 4 else 0

    def read_int64(self):
        b = self.read(8)
        return struct.unpack('<q', b)[0] if len(b) == 8 else 0

    def read_string(self):
        if self.pos + 4 > self.size: return ""
        length = self.read_int32()

        if length == 0: return ""
        if length > 10240 or length < -10240: return ""

        if length < 0:
            read_len = -length * 2
            if self.pos + read_len > self.size: return ""
            raw = self.read(read_len)
            return raw[:-2].decode('utf-16le', errors='replace')
        else:
            read_len = length
            if self.pos + read_len > self.size: return ""
            raw = self.read(read_len)
            return raw[:-1].decode('utf-8', errors='replace')


class UE4PakEngine:
    def __init__(self, pak_path):
        self.pak_path = pak_path
        self.file_size = os.path.getsize(pak_path)
        self.entries_meta = []
        self.files_map = {}
        self.mount_point = ""
        self.is_encrypted = False
        self.index_offset = 0
        self.version = 0
        self.is_old_version = False

    def xor_data(self, data):
        return bytes([b ^ XOR_KEY for b in data])

    def _read_string_from_file(self, f):
        len_b = f.read(4)
        if len(len_b) < 4: return ""
        if self.is_encrypted: len_b = self.xor_data(len_b)
        length = struct.unpack('<i', len_b)[0]

        if length == 0: return ""
        if length > 10240 or length < -10240: return ""

        if length < 0:
            read_len = -length * 2
            raw = f.read(read_len)
            if self.is_encrypted: raw = self.xor_data(raw)
            return raw[:-2].decode('utf-16le', errors='replace')
        else:
            read_len = length
            raw = f.read(read_len)
            if self.is_encrypted: raw = self.xor_data(raw)
            return raw[:-1].decode('utf-8', errors='replace')

    def parse(self):
        print(f"[Analyze] Parsing {os.path.basename(self.pak_path)}...")
        with open(self.pak_path, 'rb') as f:
            f.seek(-0x2C, 2)
            _magic = f.read(4)
            self.version = struct.unpack('<i', f.read(4))[0]

            if self.version <= 7:
                self.is_old_version = True
                print(f"[Info] Version: {self.version} (OLD Layout)")
            else:
                self.is_old_version = False
                print(f"[Info] Version: {self.version} (NEW Layout)")

            scan_size = min(self.file_size, 20 * 1024 * 1024)
            f.seek(self.file_size - scan_size)
            buffer = f.read(scan_size)

            idx = buffer.rfind(MAGIC_SEARCH_1)
            if idx != -1:
                self.is_encrypted = False
            else:
                idx = buffer.rfind(MAGIC_SEARCH_2)
                if idx != -1:
                    self.is_encrypted = True
                    print("[Info] Detected Encryption (0x79)")
                else:
                    print("[Error] Failed to locate Index Offset.")
                    return False

            self.index_offset = (self.file_size - scan_size) + idx - 4
            f.seek(self.index_offset)

            self.mount_point = self._read_string_from_file(f)
            if self.mount_point.startswith("../../../"):
                self.mount_point = self.mount_point[9:]
            print(f"[Info] Mount Point: {self.mount_point}")

            cnt_b = f.read(4)
            if self.is_encrypted: cnt_b = self.xor_data(cnt_b)
            file_count = struct.unpack('<i', cnt_b)[0]
            print(f"[Info] File Count: {file_count}")

            if self.is_old_version:
                for i in range(file_count):
                    filename = self._read_string_from_file(f)
                    full_path = os.path.join(self.mount_point, filename).replace("\\", "/")
                    meta = self._read_entry_meta(f, debug_idx=i)
                    if not meta: break
                    self.entries_meta.append(meta)
                    self.files_map[full_path] = i
            else:
                print("[Info] Reading Entries...")
                for i in range(file_count):
                    meta = self._read_entry_meta(f, debug_idx=i)
                    if not meta: break
                    self.entries_meta.append(meta)

                f.read(8)
                if self.is_encrypted: f.read(1)

                curr = f.tell()
                dir_size = self.file_size - 0x2C - curr
                if dir_size > 0:
                    print(f"[Info] Parsing Directory Index ({dir_size} bytes)...")
                    dir_data = f.read(dir_size)
                    if self.is_encrypted: dir_data = self.xor_data(dir_data)
                    self._parse_directory_original(dir_data)
                else:
                    print("[Warning] No directory index found.")

            return True

    def _read_entry_meta(self, f, debug_idx=-1):
        raw = f.read(69)
        if len(raw) < 69: return None
        if self.is_encrypted: raw = self.xor_data(raw)

        br = BinaryStream(raw)
        e = {}
        e['hash'] = br.read(20)
        e['offset'] = br.read_int64()
        e['size'] = br.read_int64()
        e['zip'] = br.read_int32()
        e['zsize'] = br.read_int64()
        e['dummy'] = br.read(21)

        e['chunks'] = []
        # if e['zip'] == 0:
        #     print("zip == 0 !!!")  # ← 新增的打印语句
        if e['zip'] != 0:
            c_cnt_bytes = f.read(4)
            if self.is_encrypted: c_cnt_bytes = self.xor_data(c_cnt_bytes)
            chunk_count = struct.unpack('<i', c_cnt_bytes)[0]

            if chunk_count < 0 or chunk_count > 50000: return None

            for _ in range(chunk_count):
                c_raw = f.read(16)
                if self.is_encrypted: c_raw = self.xor_data(c_raw)
                c_off = struct.unpack('<q', c_raw[0:8])[0]
                c_end = struct.unpack('<q', c_raw[8:16])[0]
                e['chunks'].append({'start': c_off, 'end': c_end})

        tail = f.read(5)
        if self.is_encrypted: tail = self.xor_data(tail)
        e['max_chunk_size'] = struct.unpack('<i', tail[0:4])[0]
        e['is_encrypted'] = tail[4]

        if e['max_chunk_size'] == 0: e['max_chunk_size'] = 65536
        return e

    def _parse_directory_original(self, data):
        br = BinaryStream(data)
        try:
            if br.pos >= br.size: return
            num_dirs = br.read_int64()
            print(f"[Info] Found {num_dirs} directories.")
            if num_dirs > 200000 or num_dirs < 0: return

            for _ in range(num_dirs):
                dir_name = br.read_string()
                files_count = br.read_int64()
                full_dir = dir_name

                for _ in range(files_count):
                    file_name = br.read_string()
                    idx = br.read_int32()

                    full_path = f"{full_dir}/{file_name}".replace("\\", "/")
                    while "//" in full_path:
                        full_path = full_path.replace("//", "/")
                    if full_path.startswith("../../../"):
                        full_path = full_path[9:]
                    self.files_map[full_path] = idx
        except Exception as e:
            print(f"[Error] Directory parsing failed: {e}")

    def _compress_best(self, data, target_limit):

        original_size = len(data)
        best_res = None
        best_size = sys.maxsize
        best_method = "None"

        def show_progress(msg):
            sys.stdout.write(f"\r    {msg}")
            sys.stdout.flush()

        def print_final_decision(method, size):
            sys.stdout.write("\r" + " " * 80 + "\r")

            ratio = (1 - size / original_size) * 100
            diff = size - target_limit

            status = "OK" if size <= target_limit else f"LIMIT+{diff}"
            color_code = ""

            print(f"[{method}] Size: {size} ({ratio:.1f}%) -> {status}")

        for level in range(1, 10):
            if level > 3:
                show_progress(f"> [Testing] Zlib Level {level} (Current Best: {best_size})...")

            c = zlib.compress(data, level=level)
            c_len = len(c)

            if c_len < best_size:
                best_res = c
                best_size = c_len
                best_method = f"Zlib L{level}"

            if c_len <= target_limit:
                print_final_decision(f"Zlib L{level}", c_len)
                return c

        if HAS_ZOPFLI and best_size > target_limit:
            show_progress(f"> [Zopfli] Analyzing... (Diff: {best_size - target_limit})")
            try:
                c_zopfli = zopfli.zlib.compress(data, numiterations=15)
                if len(c_zopfli) < best_size:
                    best_res = c_zopfli
                    best_size = len(c_zopfli)
                    best_method = "Zopfli"
            except Exception:
                pass

        print_final_decision(best_method, best_size)
        return best_res

    def extract(self, out_dir):
        print(f"Extracting {len(self.files_map)} files to {out_dir} ...")
        abs_out_dir = os.path.abspath(out_dir)
        with open(self.pak_path, 'rb') as f:
            for path, idx in self.files_map.items():
                if idx >= len(self.entries_meta): continue
                e = self.entries_meta[idx]
                safe_path = path.replace("\\", "/").lstrip("/")
                if not safe_path: continue
                dest = os.path.join(abs_out_dir, safe_path)
                if not os.path.abspath(dest).startswith(abs_out_dir): continue
                if os.path.isdir(dest): continue
                try:
                    os.makedirs(os.path.dirname(dest), exist_ok=True)
                    with open(dest, 'wb') as fo:
                        if len(e['chunks']) > 0:
                            for chunk in e['chunks']:
                                f.seek(chunk['start'])
                                size = chunk['end'] - chunk['start']
                                data = f.read(size)
                                if e['is_encrypted']: data = self.xor_data(data)
                                if e['zip'] != 0:
                                    try:
                                        data = zlib.decompress(data)
                                    except:
                                        pass
                                fo.write(data)
                        else:
                            f.seek(e['offset'])
                            read_size = e['zsize'] if e['zip'] != 0 else e['size']
                            data = f.read(read_size)
                            if e['is_encrypted']: data = self.xor_data(data)
                            if e['zip'] != 0:
                                try:
                                    data = zlib.decompress(data)
                                except:
                                    pass
                            fo.write(data)
                        print(f"Extracted: {safe_path}")
                except Exception as ex:
                    print(f"[Error] Failed to extract {safe_path}: {ex}")

    def reimport(self, in_dir):
        print(f"[Action] Patching from directory: {in_dir}")
        success_count = 0
        skip_count = 0

        files_to_process = []
        for root, dirs, files in os.walk(in_dir):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, in_dir).replace("\\", "/").lstrip("/")
                files_to_process.append((rel_path, full_path))

        print(f"[Info] Found {len(files_to_process)} files to patch.")

        map_keys_lower = {k.lower(): k for k in self.files_map.keys()}

        with open(self.pak_path, 'r+b') as f:
            for rel_path, full_path in files_to_process:
                search_key = rel_path.lower()
                target_key = None

                if search_key in map_keys_lower:
                    target_key = map_keys_lower[search_key]
                if not target_key:
                    for mk, original_key in map_keys_lower.items():
                        if mk.endswith("/" + search_key) or mk == search_key:
                            target_key = original_key
                            break

                if not target_key:
                    print(f"[IGNORE] {rel_path} (Not in PAK)")
                    continue

                idx = self.files_map[target_key]
                e = self.entries_meta[idx]

                with open(full_path, 'rb') as fin:
                    new_data = fin.read()

                chunk_size = e['max_chunk_size']
                if chunk_size == 0: chunk_size = 65536

                new_slices = [new_data[i:i + chunk_size] for i in range(0, len(new_data), chunk_size)]
                orig_chunks = e['chunks']

                if len(orig_chunks) == 0:
                    phys_limit = e['zsize'] if e['zip'] != 0 else e['size']
                    blob = new_data
                    if e['zip'] != 0:
                        blob = self._compress_extreme(new_data, phys_limit)
                    if e['is_encrypted']: blob = self.xor_data(blob)

                    if len(blob) > phys_limit:
                        print(f"[FAIL] {rel_path} (Overflow: {len(blob)} > {phys_limit})")
                        skip_count += 1
                        continue

                    f.seek(e['offset'])
                    f.write(blob)
                    if len(blob) < phys_limit: f.write(b'\x00' * (phys_limit - len(blob)))
                    f.flush()
                    print(f"[OK] {rel_path}")
                    success_count += 1
                    continue

                if len(new_slices) > len(orig_chunks):
                    print(f"[FAIL] {rel_path} (Chunk Count Overflow)")
                    skip_count += 1
                    continue

                write_ops = []
                possible = True

                for i in range(len(orig_chunks)):
                    orig_chunk = orig_chunks[i]
                    phys_limit = orig_chunk['end'] - orig_chunk['start']

                    if i < len(new_slices):
                        raw_slice = new_slices[i]
                        final_data = raw_slice

                        if e['zip'] != 0:
                            final_data = self._compress_best(raw_slice, phys_limit)

                        enc_data = final_data
                        if e['is_encrypted']:
                            enc_data = self.xor_data(final_data)

                        if len(enc_data) > phys_limit:
                            print(f"[FAIL] {rel_path} (Chunk {i} Overflow: {len(enc_data)} > {phys_limit})")
                            possible = False
                            break

                        write_ops.append({
                            'offset': orig_chunk['start'],
                            'data': enc_data,
                            'padding': phys_limit - len(enc_data)
                        })
                    else:
                        write_ops.append({
                            'offset': orig_chunk['start'],
                            'data': b'',
                            'padding': phys_limit
                        })

                if possible:
                    for op in write_ops:
                        f.seek(op['offset'])
                        f.write(op['data'])
                        if op['padding'] > 0: f.write(b'\x00' * op['padding'])
                        f.flush()
                    print(f"[OK] {rel_path}")
                    success_count += 1
                else:
                    skip_count += 1

        print(f"\nDone. Updated: {success_count}, Skipped/Failed: {skip_count}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=['unpack', 'reimport'])
    parser.add_argument("pak_file")
    parser.add_argument("work_dir")
    args = parser.parse_args()

    if not os.path.exists(args.pak_file):
        print("PAK file not found.")
        return

    tool = UE4PakEngine(args.pak_file)
    if tool.parse():
        if args.action == 'unpack':
            tool.extract(args.work_dir)
        elif args.action == 'reimport':
            tool.reimport(args.work_dir)


if __name__ == "__main__":
    main()
