#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export ROOT
python3 - <<'PY'
import os
import shutil
from pathlib import Path
root = Path(os.environ["ROOT"])
backup = root / "refactor_evidence" / "original"
for relative in [
    Path("README.md"),
    Path("projects/network_auth_audit_lab/README.md"),
    Path("projects/android_loader_forensics_lab/README.md"),
]:
    source = backup / relative
    target = root / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(source), str(target))
shutil.rmtree(str(root / "projects/kernel_memory_defense_lab"), ignore_errors=True)
for relative in [
    "projects/network_auth_audit_lab/protocol_codec.py",
    "projects/network_auth_audit_lab/loopback_simulator.py",
    "projects/network_auth_audit_lab/mutation_test.py",
    "projects/network_auth_audit_lab/tests",
]:
    path = root / relative
    if path.is_dir():
        shutil.rmtree(str(path))
    elif path.exists():
        path.unlink()
print("ROLLBACK_RESULT=原 README 与两个子项目说明已恢复；新增内核实验室、协议编解码、回环服务和变异测试已移除")
PY
