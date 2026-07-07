# get\_input\_desc

## 产品支持情况

全量芯片支持。

## 功能说明

获取指定输入端口的Tensor格式。

## 函数原型

```python
get_input_desc(index: int) -> TensorDesc
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 输入索引。 |

## 返回值说明

获取到的Tensor格式。

## 约束说明

- 如果index不是整数，抛出TypeError。
- 如果Tensor格式获取失败，抛出RuntimeError。
