#!/usr/bin/env python3
"""只读 ABI 路由验证：选择并校验合成描述文件，不解码或加载内核模块。"""
import argparse
import hashlib
import json
import platform
from pathlib import Path

DEFAULT_MANIFEST = Path(__file__).with_name("samples") / "abi_manifest.json"


def select(manifest: dict, system: str, release: str):
    candidates = [x for x in manifest["artifacts"] if x["system"] == system]
    exact = [x for x in candidates if x["release"] == release]
    if exact:
        return exact[0], "exact"
    family = release.split(".")[:2]
    compatible = [x for x in candidates if x["release"].split(".")[:2] == family]
    return (compatible[0], "family") if compatible else (None, "none")


def main() -> int:
    parser = argparse.ArgumentParser(description="验证合成 ABI 清单路由，不加载任何驱动。")
    parser.add_argument("manifest", nargs="?", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--system", default="Linux")
    parser.add_argument("--release", default="6.1.55-lab")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    item, match = select(manifest, args.system, args.release)
    if item is None:
        print("平台=%s 内核=%s 匹配=none 当前主机=%s/%s" % (args.system, args.release, platform.system(), platform.release()))
        return 2
    digest = hashlib.sha256(item["descriptor"].encode("utf-8")).hexdigest()
    print("平台=%s 内核=%s 匹配=%s 描述=%s SHA256=%s" % (args.system, args.release, match, item["descriptor"], digest))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
