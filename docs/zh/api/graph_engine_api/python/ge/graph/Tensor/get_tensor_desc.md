# get\_tensor\_desc

## 产品支持情况

全量芯片支持。

## 功能说明

获取Tensor的描述符。

## 函数原型

```python
get_tensor_desc() -> TensorDesc
```

## 参数说明

无。

## 返回值说明

返回当前Tensor的描述符。

## 约束说明

- 如果Tensor格式获取失败，抛出RuntimeError。
- 修改返回的TensorDesc信息，不影响Tensor对象中已有的TensorDesc信息。
