# to\_host

## 产品支持情况

全量芯片支持。

## 功能说明

将当前Tensor从Device移动到Host。

## 函数原型

```python
to_host() -> Tensor
```

## 参数说明

无

## 返回值说明

\(Tensor\) 返回自身以支持链式调用。

## 约束说明

- 如果当前Tensor不是Device内存，抛出ValueError。
- 如果移动失败，抛出RuntimeError。
