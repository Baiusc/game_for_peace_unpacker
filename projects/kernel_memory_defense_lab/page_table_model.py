'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2026-09-08 12:03:54
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2026-09-08 13:19:31
FilePath     : /game_for_peace_unpacker/projects/kernel_memory_defense_lab/page_table_model.py
Description  : 

Copyright (c) 2026 by vitalchem, All Rights Reserved. 
'''
#!/usr/bin/env python3
"""合成四级页表与只读跨页读取模型；不访问真实进程或物理内存。"""
import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple

PAGE_SHIFT = 12
PAGE_SIZE = 1 << PAGE_SHIFT
INDEX_MASK = 0x1FF
DEFAULT_FIXTURE = Path(__file__).with_name("samples") / "page_table_fixture.json"


@dataclass(frozen=True)
class Translation:
    virtual_address: int
    physical_address: int
    indices: Tuple[int, int, int, int]


class SyntheticAddressSpace:
    """以稀疏字典模拟 PGD/PUD/PMD/PTE，仅允许读取 fixture 声明的页面。"""

    def __init__(self, pages: Dict[int, bytes], mappings: Dict[int, int]):
        self.pages = pages
        self.mappings = mappings

    @staticmethod
    def indices(vaddr: int) -> Tuple[int, int, int, int]:
        return tuple((vaddr >> shift) & INDEX_MASK for shift in (39, 30, 21, 12))

    def translate(self, vaddr: int) -> Translation:
        vpn = vaddr >> PAGE_SHIFT
        if vpn not in self.mappings:
            raise KeyError("PTE_NOT_PRESENT")
        pfn = self.mappings[vpn]
        return Translation(vaddr, (pfn << PAGE_SHIFT) | (vaddr & (PAGE_SIZE - 1)), self.indices(vaddr))

    def read(self, vaddr: int, size: int) -> bytes:
        if size < 0 or size > 1024 * 1024:
            raise ValueError("READ_SIZE_OUT_OF_RANGE")
        result = bytearray()
        cursor = vaddr
        while len(result) < size:
            translated = self.translate(cursor)
            pfn = translated.physical_address >> PAGE_SHIFT
            offset = translated.physical_address & (PAGE_SIZE - 1)
            page = self.pages.get(pfn)
            if page is None:
                raise KeyError("PHYSICAL_PAGE_MISSING")
            take = min(size - len(result), PAGE_SIZE - offset)
            result.extend(page[offset : offset + take])
            cursor += take
        return bytes(result)


def load_fixture(path: Path) -> Tuple[str, SyntheticAddressSpace, int, int]:
    fixture = json.loads(path.read_text(encoding="utf-8"))
    if fixture.get("scope") != "synthetic-only":
        raise ValueError("FIXTURE_SCOPE_REJECTED")
    pages = {}
    mappings = {}
    for item in fixture["pages"]:
        pfn = int(item["pfn"], 0)
        payload = bytes.fromhex(item["data_hex"])
        offset = item.get("offset", 0)
        page = bytearray(PAGE_SIZE)
        page[offset : offset + len(payload)] = payload
        pages[pfn] = bytes(page)
        mappings[int(item["vaddr"], 0) >> PAGE_SHIFT] = pfn
    return fixture["process"], SyntheticAddressSpace(pages, mappings), int(fixture["read_vaddr"], 0), fixture["read_size"]


def main() -> int:
    parser = argparse.ArgumentParser(description="运行合成四级页表只读访问演示。")
    parser.add_argument("fixture", nargs="?", type=Path, default=DEFAULT_FIXTURE)
    args = parser.parse_args()
    process, space, vaddr, size = load_fixture(args.fixture)
    translation = space.translate(vaddr)
    data = space.read(vaddr, size)
    print("对象=%s" % process)
    print("页表路径=PGD[%d]->PUD[%d]->PMD[%d]->PTE[%d]" % translation.indices)
    print("虚拟地址=0x%x 物理地址=0x%x 读取字节=%d" % (vaddr, translation.physical_address, len(data)))
    print("结果=%s" % data.decode("utf-8"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
