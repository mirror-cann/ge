# set\_format

## 产品支持情况

全量芯片支持。

## 功能说明

设置张量数据格式。

## 函数原型

```python
set_format(format: Format) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| format | 输入 | 数据格式枚举。详见[Format](../../../../python/ge/Format.md)。 |

## 返回值说明

\(TensorHolder\)返回自身以支持链式调用。

## 约束说明

- 如果format不是Format枚举，抛出TypeError。
- 如果操作失败，抛出RuntimeError。
