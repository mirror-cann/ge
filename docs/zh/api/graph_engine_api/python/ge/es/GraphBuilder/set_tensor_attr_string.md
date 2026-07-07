# set\_tensor\_attr\_string

## 产品支持情况

全量芯片支持。

## 功能说明

为张量设置string属性。

## 函数原型

```python
set_tensor_attr_string(tensor: TensorHolder, attr_name: str, value: str) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| tensor | 输入 | 张量持有者对象。 |
| attr_name | 输入 | 属性名称。 |
| value | 输入 | 字符串值。 |

## 返回值说明

无

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果设置失败，抛出RuntimeError。
