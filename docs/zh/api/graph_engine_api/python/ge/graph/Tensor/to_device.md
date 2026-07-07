# to\_device

## 产品支持情况

全量芯片支持。

## 功能说明

将当前Tensor从Host移动到Device。

## 函数原型

```python
to_device() -> Tensor
```

## 参数说明

无

## 返回值说明

\(Tensor\) 返回自身以支持链式调用。

## 约束说明

- 如果当前Tensor不是Host内存，抛出ValueError。
- 如果移动失败，抛出RuntimeError。
