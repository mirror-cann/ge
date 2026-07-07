# \_\_init\_\_

## 产品支持情况

全量芯片支持。

## 功能说明

构造SubgraphInput。支持两种构造方式：构造空输入（随后通过add\_input追加锚点），或从 \(node, out\_index\) 可迭代对象构造。

## 函数原型

```python
__init__() -> None
__init__(node_inputs: Iterable[tuple[Node, int]]) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node_inputs | 输入 | 可选。(node, out_index) 可迭代对象；每个元素必须是2元组，否则C++层抛出RuntimeError。 |

## 返回值说明

无

## 约束说明

当传入node\_inputs时，每个元素必须是2元组，否则C++层抛出RuntimeError。
