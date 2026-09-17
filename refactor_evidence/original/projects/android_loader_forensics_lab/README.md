# Android 加载器静态取证实验室

本子项目参考《某 PUBG 内核辅助逆向分析》中“自解压包装、hex 编码嵌入载荷、短生命周期文件和隐藏行为”的分析路径，转化为**不执行样本的静态取证练习**。它只针对受控二进制副本查找 hex 编码的 ELF 片段，导出用于哈希、文件类型和字符串审计的静态副本。

## 运行

```bash
python3 extract_hex_elf.py
```

默认输入 `samples/synthetic_loader.bin`，默认输出 `analysis_output/embedded_00.elf` 与 `analysis_output/manifest.tsv`。样本为合成数据；脚本没有 `subprocess`、`os.system`、加载器调用或设备访问逻辑。

## 实践流程

1. 保留原始样本副本并计算其 SHA-256。
2. 在字节流中定位以 `7f454c46` 开头的连续十六进制文本。
3. 解码为静态副本，为每个副本记录字节数和 SHA-256。
4. 在隔离环境中使用只读工具进一步检查 ELF 头、字符串、导入表与可疑行为指示符。
5. 将发现映射到防护检测点，例如异常临时文件、自删除痕迹、未知设备节点、模块完整性异常或可疑覆盖层调用。

## 验证

```bash
python3 extract_hex_elf.py
test -s analysis_output/embedded_00.elf
test -s analysis_output/manifest.tsv
```

## 边界

本工具不得执行、加载、安装、重打包或分发提取结果；它仅服务于样本保全和静态分析。
