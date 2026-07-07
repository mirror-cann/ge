# set\_data\_type

## 产品支持情况

全量芯片支持。

## 功能说明

向TensorDesc中设置Tensor的数据类型。

## 函数原型

```python
set_data_type(data_type: DataType) -> TensorDesc
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| data_type | 输入 | 需设置的TensorDesc所描述的Tensor的数据类型信息。 |

## 返回值说明

\(TensorDesc\) 返回自身以支持链式调用。

## 约束说明

如果data\_type不是DataType类型，抛出TypeError。
