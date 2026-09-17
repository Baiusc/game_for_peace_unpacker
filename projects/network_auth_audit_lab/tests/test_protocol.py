#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from loopback_simulator import COMM_KEY, SOFT_CODE, SOFT_SECRET, ProtocolState, pack, unpack
from protocol_codec import legacy_sign, rc4_crypt


class ProtocolTests(unittest.TestCase):
    def test_rc4_known_vector(self):
        self.assertEqual(rc4_crypt(b"Key", b"Plaintext").hex(), "bbf316e8d940af0ad3")

    def test_packet_roundtrip_and_signature(self):
        payload = {"action": "init", "soft_code": SOFT_CODE, "timestamp": 1, "nonce": "n"}
        self.assertEqual(unpack(pack(payload)), payload)
        envelope = pack(payload)
        envelope["sign"] = "0" * 32
        with self.assertRaisesRegex(ValueError, "BAD_SIGNATURE"):
            unpack(envelope)

    def test_freshness_and_replay(self):
        state = ProtocolState(clock=lambda: 100)
        payload = {"action": "init", "soft_code": SOFT_CODE, "timestamp": 100, "nonce": "n"}
        self.assertTrue(state.handle(payload)["ok"])
        self.assertEqual(state.handle(payload)["error"], "REPLAY_OR_MISSING_NONCE")
        self.assertEqual(state.handle(dict(payload, nonce="n2", timestamp=1))["error"], "STALE_TIMESTAMP")

    def test_synthetic_constants(self):
        self.assertTrue(SOFT_CODE.startswith("LAB"))
        self.assertTrue(SOFT_SECRET.startswith("lab-"))
        self.assertTrue(COMM_KEY.startswith(b"lab-"))
        self.assertEqual(len(legacy_sign("x", SOFT_SECRET)), 32)


if __name__ == "__main__":
    unittest.main(verbosity=2)
