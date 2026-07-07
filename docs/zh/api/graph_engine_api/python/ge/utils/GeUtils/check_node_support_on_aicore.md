# check\_node\_support\_on\_aicore

## 产品支持情况

全量芯片支持。

## 功能说明

对传入的Node做校验，校验其是否支持在AI Core上执行。

## 函数原型

```python
check_node_support_on_aicore(node: Node) -> Tuple[bool, str]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| node | 输入 | 待检查的节点，类型为Node。 |

## 返回值说明

- \(Tuple\[bool, str\]\) 返回\(is\_supported, unsupported\_reason\)元组。
- is\_supported为True表示是否支持在AI Core上执行，False表示不支持。
- unsupported\_reason为不支持的具体原因。

## 约束说明

- 如果检查操作失败，抛出RuntimeError。
- 在调用此接口之前，请确保节点的shape已被推导过。因为校验过程需要结合shape信息进行判断。
