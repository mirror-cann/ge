# set\_format

## 产品支持情况

全量芯片支持。

## 功能说明

设置张量格式。

## 函数原型

```python
set_format(format: Format) -> Tensor
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| format | 输入 | 要设置的格式。详细格式请参见[Format](../../../ge/Format.md)。 |

## 返回值说明

\(Tensor\) 返回自身以支持链式调用。

## 约束说明

- 如果format不是Format类型，抛出TypeError。
- 如果设置失败，抛出RuntimeError。
