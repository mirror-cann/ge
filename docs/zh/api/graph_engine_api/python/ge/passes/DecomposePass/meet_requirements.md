# meet\_requirements

## 产品支持情况

全量芯片支持。

## 功能说明

判断节点是否需要分解。为可选实现，默认返回True。

## 函数原型

```python
meet_requirements(self, node: Node) -> bool
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 待判断的节点，类型为ge.graph.Node。 |

## 返回值说明

返回True表示需要分解，将执行替换；返回False表示不需要分解，跳过。默认返回True。

## 约束说明

无
