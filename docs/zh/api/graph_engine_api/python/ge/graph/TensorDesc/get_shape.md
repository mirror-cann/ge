# get\_shape

## 产品支持情况

全量芯片支持。

## 功能说明

获取TensorDesc所描述Tensor的Shape。

## 函数原型

```python
get_shape() -> Shape
```

## 参数说明

无

## 返回值说明

Shape对象。

## 约束说明

由于返回的Shape信息为值拷贝，因此修改返回的Shape信息，不影响TensorDesc中已有的Shape信息。
