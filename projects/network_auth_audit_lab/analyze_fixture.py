#!/usr/bin/env python3
"""离线审计合成认证轨迹；默认读取同目录 samples/auth_trace.json。"""
import argparse
import json
from typing import Dict, List
from pathlib import Path

DEFAULT_INPUT = Path(__file__).with_name("samples") / "auth_trace.json"


def assess(trace: dict) -> List[Dict]:
    findings = []
    for message in trace.get("messages", []):
        phase = message.get("phase", "unknown")
        if message.get("client_secret_storage") == "embedded_static":
            findings.append({"phase": phase, "severity": "高", "issue": "客户端内嵌静态共享密钥", "remediation": "将长期密钥移出客户端；使用服务端保管的密钥和短期凭据。"})
        if message.get("nonce") == "missing":
            findings.append({"phase": phase, "severity": "高", "issue": "请求缺少一次性随机数", "remediation": "加入服务端保存的 nonce、时效窗口和重放拒绝策略。"})
        if message.get("response_signature_verified") is False:
            findings.append({"phase": phase, "severity": "高", "issue": "客户端未验证服务端响应完整性", "remediation": "对响应执行签名或 MAC 验证，并将失败作为终止条件。"})
        if message.get("session_binding") == "none":
            findings.append({"phase": phase, "severity": "中", "issue": "会话未绑定到认证上下文", "remediation": "绑定账户、设备证明、会话过期时间和令牌受众。"})
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description="离线审计合成认证轨迹，不执行网络请求。")
    parser.add_argument("input", nargs="?", type=Path, default=DEFAULT_INPUT, help="JSON 轨迹文件")
    args = parser.parse_args()
    trace = json.loads(args.input.read_text(encoding="utf-8"))
    findings = assess(trace)
    print(f"样本：{trace.get('sample', args.input.name)}")
    print(f"传输：{trace.get('transport', 'unknown')}；发现：{len(findings)} 项")
    for item in findings:
        print(f"[{item['severity']}] {item['phase']}：{item['issue']}\n  整改：{item['remediation']}")
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
