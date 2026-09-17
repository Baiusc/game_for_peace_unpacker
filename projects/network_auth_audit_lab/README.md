# 应用层协议兼容性与健壮性实验室

本子项目根据文献中的 `init -> login`、RC4、盐值加 MD5 签名等遗留设计，构造与任何真实服务无关的本地协议 fixture。它同时保留离线风险审计，并新增回环收发、签名校验、时间戳新鲜度、nonce 去重和字段变异测试。

## 运行

```bash
python3 analyze_fixture.py
python3 loopback_simulator.py
python3 mutation_test.py
python3 -m unittest discover -s tests -v
```

`analyze_fixture.py` 默认读取 `samples/auth_trace.json`，发现风险时以退出码 `1` 表示需要整改。`loopback_simulator.py` 只绑定系统分配的 `127.0.0.1` TCP 端口，并在一个进程内完成 init/login 收发。`mutation_test.py` 使用 `LAB` 前缀合成密钥，验证错误软件密钥、错误通讯密钥、过期时间戳与 nonce 重放。

## 实践目标

1. 将认证流程建模为 `init -> login -> session` 的消息序列。
2. 审计每条消息的长期密钥暴露、nonce、时效、请求/响应完整性和会话绑定。
3. 输出可落地的整改项：服务端持有长期机密、短时令牌、双向完整性验证、nonce 去重及最小权限会话。

## 验证

```bash
python3 analyze_fixture.py; test $? -eq 1
```

预期输出为 5 项风险；这是合成样本刻意包含的问题，不代表任何真实服务的结论。

## 输入、输出与边界

默认输入均为代码内或 `samples/` 中的合成数据；输出是控制台测试结果。网络目标固定为 IPv4 回环地址，未实现 HTTP、域名、代理、端口扫描或第三方管理接口。RC4/MD5 仅用于兼容性检测，响应仍强制验签并加入时间戳与 nonce 校验。
