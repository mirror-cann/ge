# add\_control\_dependency

## 产品支持情况

全量芯片支持。

## 功能说明

从src\_tensors向dst\_tensor添加控制依赖。

## 函数原型

```python
add_control_dependency(dst_tensor: TensorHolder, src_tensors: List[TensorHolder]) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| dst_tensor | 输入 | 目标张量持有者。 |
| src_tensors | 输入 | 源张量持有者列表。 |

## 返回值说明

无

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果添加失败，抛出RuntimeError。
