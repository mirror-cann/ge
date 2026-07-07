# build\_and\_reset

## 产品支持情况

全量芯片支持。

## 功能说明

构建图。

## 函数原型

```python
build_and_reset(outputs: Optional[List[TensorHolder]] = None) -> Graph
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| outputs | 输入 | 可选参数，要设置为图输出的张量持有者列表。如果提供，会自动设置所有输出后再构建图。输出索引从0开始顺序分配。如果为None，则使用之前设置的输出构建图。 |

## 返回值说明

\(Graph\) 构建好的图对象。

## 约束说明

- 调用build\_and\_reset\(\)后，构建器进入已构建状态，不能用于创建新张量。
- 需要创建新的GraphBuilder来构建另一个图。
- 如果outputs不是TensorHolder对象列表，抛出TypeError。
- 如果构建失败，抛出RuntimeError。
