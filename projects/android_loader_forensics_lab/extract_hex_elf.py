#!/usr/bin/env python3
"""从离线样本提取 hex 编码 ELF 片段；只写出静态分析副本，绝不执行。"""
import argparse
import hashlib
import re
from typing import List
from pathlib import Path

DEFAULT_INPUT = Path(__file__).with_name("samples") / "synthetic_loader.bin"
DEFAULT_OUTPUT = Path(__file__).with_name("analysis_output")
HEX_ELF = re.compile(rb"7f454c46(?:[0-9a-fA-F]{2}){12,}")


def extract(data: bytes) -> List[bytes]:
    blobs = []
    for match in HEX_ELF.finditer(data):
        try:
            decoded = bytes.fromhex(match.group().decode("ascii"))
        except ValueError:
            continue
        if decoded.startswith(b"\x7fELF"):
            blobs.append(decoded)
    return blobs


def main() -> int:
    parser = argparse.ArgumentParser(description="离线提取 hex-ELF 载荷，不执行、不加载任何输出。")
    parser.add_argument("input", nargs="?", type=Path, default=DEFAULT_INPUT, help="待分析的本地样本")
    parser.add_argument("-o", "--output", type=Path, default=DEFAULT_OUTPUT, help="静态副本输出目录")
    args = parser.parse_args()
    payloads = extract(args.input.read_bytes())
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for index, payload in enumerate(payloads):
        output = args.output / f"embedded_{index:02d}.elf"
        output.write_bytes(payload)
        manifest.append(f"{output.name}\t{len(payload)}\t{hashlib.sha256(payload).hexdigest()}")
    (args.output / "manifest.tsv").write_text("文件\t字节数\tSHA256\n" + "\n".join(manifest) + "\n", encoding="utf-8")
    print(f"输入：{args.input}；提取：{len(payloads)} 个静态 ELF 片段；输出：{args.output}")
    return 0 if payloads else 1


if __name__ == "__main__":
    raise SystemExit(main())
