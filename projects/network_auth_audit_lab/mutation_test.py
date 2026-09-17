#!/usr/bin/env python3
"""对本地回环模拟器执行密钥、签名、时间戳与 nonce 变异测试。"""
import json
import threading

from loopback_simulator import COMM_KEY, SOFT_CODE, SOFT_SECRET, LoopbackServer, ProtocolState, exchange


def run_mutations():
    now = 2000000000
    server = LoopbackServer(state=ProtocolState(clock=lambda: now))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    port = server.server_address[1]
    results = {}
    base = {"action": "login", "soft_code": SOFT_CODE, "timestamp": now, "nonce": "base-nonce"}
    try:
        results["baseline"] = exchange(port, base)["ok"]
        results["replay"] = exchange(port, base)["error"]
        stale = dict(base, nonce="stale-nonce", timestamp=now - 120)
        results["stale_timestamp"] = exchange(port, stale)["error"]
        results["soft_secret_mutation"] = exchange(
            port, dict(base, nonce="secret-mutation"), soft_secret=SOFT_SECRET + "X"
        )["error"]
        results["comm_key_mutation"] = exchange(
            port, dict(base, nonce="key-mutation"), comm_key=COMM_KEY[:-1] + b"X"
        )["error"]
        return results
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


def main() -> int:
    results = run_mutations()
    print(json.dumps(results, ensure_ascii=False, sort_keys=True))
    expected = {
        "baseline": True,
        "replay": "REPLAY_OR_MISSING_NONCE",
        "stale_timestamp": "STALE_TIMESTAMP",
        "soft_secret_mutation": "BAD_SIGNATURE",
        "comm_key_mutation": "DECRYPT_OR_JSON_ERROR",
    }
    return 0 if results == expected else 1


if __name__ == "__main__":
    raise SystemExit(main())
