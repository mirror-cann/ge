# get\_captured\_tensors

## 产品支持情况

全量芯片支持。

## 功能说明

返回已捕获的Tensor列表。

## 函数原型

```python
get_captured_tensors() -> list[NodeIo]
```

## 参数说明

无

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| list[NodeIo] | 以NodeIo（包含node与index）形式返回已捕获的Tensor列表。 |

## 约束说明

无
