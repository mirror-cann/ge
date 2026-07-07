# capture\_tensor

## 产品支持情况

全量芯片支持。

## 功能说明

在模式图中标记需要捕获的Tensor。被捕获的Tensor会按调用顺序保存，后续可通过match\_result.get\_captured\_tensor\(index\) 读取。

## 函数原型

```python
capture_tensor(source: Union[TensorHolder, Node, NodeIo], index: Optional[int] = None) -> Pattern
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| source | 输入 | 需要捕获的Tensor来源。支持TensorHolder、Node或NodeIo。 |
| index | 输入 | 输出索引。当source为TensorHolder或Node时可指定输出索引，未指定时默认为0；当source为NodeIo时不需要传入。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| Pattern | 返回当前Pattern，支持链式调用。 |

## 约束说明

无

## 调用示例

```python
from ge.passes import create_pattern

# builder为模式图构造器，add、matmul为模式图中已构建的节点
pat = create_pattern(builder.build_and_reset([add]))
pat.capture_tensor(matmul)
pat.capture_tensor(add)
```
