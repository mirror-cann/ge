# get\_data\_type

## 产品支持情况

全量芯片支持。

## 功能说明

获取TensorDesc所描述Tensor的数据类型。

## 函数原型

```python
get_data_type() -> DataType
```

## 参数说明

无

## 返回值说明

DataType枚举值。

## 约束说明

由于返回的DataType信息为值拷贝，因此修改返回的DataType信息，不影响TensorDesc中已有的DataType信息。
