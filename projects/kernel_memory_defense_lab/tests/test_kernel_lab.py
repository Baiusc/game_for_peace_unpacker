#!/usr/bin/env python3
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from abi_router import select
from module_visibility_audit import audit
from page_table_model import PAGE_SIZE, SyntheticAddressSpace


class KernelLabTests(unittest.TestCase):
    def test_cross_page_read_and_indices(self):
        space = SyntheticAddressSpace({1: b"A" * PAGE_SIZE, 9: b"B" * PAGE_SIZE}, {0x10: 1, 0x11: 9})
        self.assertEqual(space.read(0x10FFF, 2), b"AB")
        self.assertEqual(space.translate(0x10FFF).indices, (0, 0, 0, 16))

    def test_unmapped_page_is_rejected(self):
        with self.assertRaisesRegex(KeyError, "PTE_NOT_PRESENT"):
            SyntheticAddressSpace({}, {}).read(0x1000, 1)

    def test_visibility_correlation(self):
        snapshot = json.loads((ROOT / "samples" / "visibility_snapshot.json").read_text())
        self.assertEqual(len(audit(snapshot)), 4)

    def test_abi_route_exact_and_family(self):
        manifest = json.loads((ROOT / "samples" / "abi_manifest.json").read_text())
        self.assertEqual(select(manifest, "Linux", "6.1.55-lab")[1], "exact")
        self.assertEqual(select(manifest, "Linux", "6.1.99-lab")[1], "family")


if __name__ == "__main__":
    unittest.main(verbosity=2)
