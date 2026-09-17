# 内核内存与隐蔽行为防御实验室

该子项目把文献中的四级页表遍历、物理页读取、模块链表摘除、sysfs 删除及多内核路由转化为可重复的**用户态合成模型和检测器**，面向 Linux/Windows 防御原型设计。

## 默认运行

```bash
python3 page_table_model.py
python3 abi_router.py
python3 module_visibility_audit.py; test $? -eq 1
python3 -m unittest discover -s tests -v
```

输入均位于 `samples/`。`page_table_model.py` 对两个不连续 PFN 执行跨页只读操作，并打印 PGD/PUD/PMD/PTE 索引；`abi_router.py` 只选择描述符并计算哈希；`module_visibility_audit.py` 关联模块列表、sysfs、设备节点、删除事件和 ioremap 调用源。

## 输出与验证含义

- 页表模型预期读出 `PAGE_TWO`，证明虚拟连续并不要求 PFN 连续。
- ABI 路由预期得到 `exact`，未知版本返回 `2`。
- 可见性快照刻意包含 4 个检测信号，返回 `1` 表示检测器命中。

## 学习边界

实现不打开 `/dev/mem`、`/proc/*/mem`，不调用 `process_vm_readv`、`ioremap`、驱动加载或模块隐藏接口，不包含 ioctl 内存读写通道。它只消费仓库内合成页、描述符和事件快照，用于验证检测逻辑和 ABI 路由策略。
