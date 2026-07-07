# get\_format

## 产品支持情况

全量芯片支持。

## 功能说明

获取TensorDesc所描述的Tensor的Format。

## 函数原型

```python
get_format() -> Format
```

## 参数说明

无

## 返回值说明

Format枚举值。

## 约束说明

由于返回的Format信息为值拷贝，因此修改返回的Format信息，不影响TensorDesc中已有的Format信息。
