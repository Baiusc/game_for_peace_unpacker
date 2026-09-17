#!/usr/bin/env python3
"""合成遗留协议的 RC4 与 MD5 兼容性实现。"""
import hashlib


def rc4_crypt(key: bytes, data: bytes) -> bytes:
    if not key:
        raise ValueError("EMPTY_RC4_KEY")
    state = list(range(256))
    j = 0
    for i in range(256):
        j = (j + state[i] + key[i % len(key)]) & 0xFF
        state[i], state[j] = state[j], state[i]
    i = j = 0
    out = bytearray()
    for value in data:
        i = (i + 1) & 0xFF
        j = (j + state[i]) & 0xFF
        state[i], state[j] = state[j], state[i]
        out.append(value ^ state[(state[i] + state[j]) & 0xFF])
    return bytes(out)


def legacy_sign(ciphertext_b64: str, soft_secret: str, salt=("LAB123", "LAB456", "LAB789")) -> str:
    material = salt[0] + ciphertext_b64 + salt[1] + soft_secret + salt[2]
    return hashlib.md5(material.encode("utf-8")).hexdigest()
