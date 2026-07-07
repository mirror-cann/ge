# get\_output\_desc

## 产品支持情况

全量芯片支持。

## 功能说明

获取指定输出端口的Tensor格式。

## 函数原型

```python
get_output_desc(index: int) -> TensorDesc
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 算子的输出端口号。 |

## 返回值说明

获取到的Tensor格式。

## 约束说明

- 如果index不是整数，抛出TypeError。
- 如果Tensor格式获取失败，抛出RuntimeError。
