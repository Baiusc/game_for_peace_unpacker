#!/usr/bin/env python3
"""仅绑定 127.0.0.1 的 init/login 协议模拟器及默认收发自测。"""
import argparse
import base64
import hashlib
import json
import socket
import socketserver
import threading
import time
from typing import Optional

from protocol_codec import legacy_sign, rc4_crypt

SOFT_CODE = "LABSOFTCODE0000001"
SOFT_SECRET = "lab-soft-secret-32-bytes-value!!"
COMM_KEY = b"lab-rc4-communication-key-48-bytes-fixture-000001"
MAX_SKEW_SECONDS = 30


def pack(payload: dict, comm_key: bytes = COMM_KEY, soft_secret: str = SOFT_SECRET) -> dict:
    plain = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ciphertext = base64.b64encode(rc4_crypt(comm_key, plain)).decode("ascii")
    return {"data": ciphertext, "sign": legacy_sign(ciphertext, soft_secret)}


def unpack(envelope: dict, comm_key: bytes = COMM_KEY, soft_secret: str = SOFT_SECRET) -> dict:
    ciphertext = envelope["data"]
    if legacy_sign(ciphertext, soft_secret) != envelope.get("sign"):
        raise ValueError("BAD_SIGNATURE")
    return json.loads(rc4_crypt(comm_key, base64.b64decode(ciphertext)).decode("utf-8"))


class ProtocolState:
    def __init__(self, clock=None):
        self.clock = clock or (lambda: int(time.time()))
        self.nonces = set()
        self.tokens = {}

    def handle(self, payload: dict) -> dict:
        now = self.clock()
        if abs(now - int(payload.get("timestamp", 0))) > MAX_SKEW_SECONDS:
            return {"ok": False, "error": "STALE_TIMESTAMP", "timestamp": now}
        nonce = payload.get("nonce")
        if not nonce or nonce in self.nonces:
            return {"ok": False, "error": "REPLAY_OR_MISSING_NONCE", "timestamp": now}
        self.nonces.add(nonce)
        if payload.get("soft_code") != SOFT_CODE:
            return {"ok": False, "error": "UNKNOWN_SOFT_CODE", "timestamp": now}
        if payload.get("action") == "init":
            return {"ok": True, "phase": "init", "server_nonce": "srv-" + nonce, "timestamp": now}
        if payload.get("action") == "login":
            token = hashlib.sha256((nonce + ":lab-token").encode()).hexdigest()[:24]
            self.tokens[token] = now + 60
            return {"ok": True, "phase": "login", "token": token, "expires_at": now + 60, "timestamp": now}
        return {"ok": False, "error": "UNKNOWN_ACTION", "timestamp": now}


class _Handler(socketserver.StreamRequestHandler):
    def handle(self):
        try:
            envelope = json.loads(self.rfile.readline().decode("utf-8"))
            payload = unpack(envelope)
            response = self.server.state.handle(payload)
        except UnicodeDecodeError:
            response = {"ok": False, "error": "DECRYPT_OR_JSON_ERROR", "timestamp": self.server.state.clock()}
        except json.JSONDecodeError:
            response = {"ok": False, "error": "DECRYPT_OR_JSON_ERROR", "timestamp": self.server.state.clock()}
        except (KeyError, ValueError) as exc:
            response = {"ok": False, "error": str(exc), "timestamp": self.server.state.clock()}
        self.wfile.write((json.dumps(pack(response), separators=(",", ":")) + "\n").encode("utf-8"))


class LoopbackServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

    def __init__(self, address=("127.0.0.1", 0), state: Optional[ProtocolState] = None):
        if address[0] != "127.0.0.1":
            raise ValueError("LOOPBACK_ONLY")
        self.state = state or ProtocolState()
        super().__init__(address, _Handler)


def exchange(port: int, payload: dict, comm_key: bytes = COMM_KEY, soft_secret: str = SOFT_SECRET) -> dict:
    with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
        sock.sendall((json.dumps(pack(payload, comm_key, soft_secret)) + "\n").encode("utf-8"))
        response = b""
        while not response.endswith(b"\n"):
            response += sock.recv(4096)
    return unpack(json.loads(response.decode("utf-8")), COMM_KEY, SOFT_SECRET)


def self_test() -> int:
    now = 2000000000
    server = LoopbackServer(state=ProtocolState(clock=lambda: now))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    port = server.server_address[1]
    try:
        init = exchange(port, {"action": "init", "soft_code": SOFT_CODE, "timestamp": now, "nonce": "nonce-init"})
        login = exchange(port, {"action": "login", "soft_code": SOFT_CODE, "timestamp": now, "nonce": "nonce-login"})
        print("传输=tcp://127.0.0.1:<ephemeral> init=%s login=%s token_len=%d" % (init["ok"], login["ok"], len(login["token"])))
        return 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


def main() -> int:
    parser = argparse.ArgumentParser(description="运行本地回环协议模拟器自测。")
    parser.parse_args()
    return self_test()


if __name__ == "__main__":
    raise SystemExit(main())
