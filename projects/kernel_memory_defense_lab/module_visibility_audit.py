#!/usr/bin/env python3
"""比对合成模块可见性快照，识别链表/sysfs/设备节点不一致。"""
import argparse
import json
from pathlib import Path

DEFAULT_INPUT = Path(__file__).with_name("samples") / "visibility_snapshot.json"


def audit(snapshot: dict):
    loaded = set(snapshot.get("module_list", []))
    sysfs = set(snapshot.get("sysfs_modules", []))
    findings = []
    for node in snapshot.get("device_nodes", []):
        owner = node.get("claimed_module")
        if owner and (owner not in loaded or owner not in sysfs):
            findings.append("DEVICE_WITHOUT_VISIBLE_MODULE:%s:%s" % (node["path"], owner))
    events = snapshot.get("events", [])
    if "module_list_unlink" in events:
        findings.append("MODULE_LIST_UNLINK_EVENT")
    if "module_kobject_delete" in events:
        findings.append("MODULE_KOBJECT_DELETE_EVENT")
    allowed = set(snapshot.get("allowed_ioremap_callers", []))
    for caller in snapshot.get("ioremap_callers", []):
        if caller not in allowed:
            findings.append("UNEXPECTED_IOREMAP_CALLER:%s" % caller)
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description="审计本地合成模块可见性快照。")
    parser.add_argument("snapshot", nargs="?", type=Path, default=DEFAULT_INPUT)
    args = parser.parse_args()
    data = json.loads(args.snapshot.read_text(encoding="utf-8"))
    findings = audit(data)
    print("快照=%s 发现=%d" % (data.get("sample"), len(findings)))
    for finding in findings:
        print("检测=%s" % finding)
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
