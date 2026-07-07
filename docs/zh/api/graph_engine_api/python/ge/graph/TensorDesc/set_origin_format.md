# set\_origin\_format

## 产品支持情况

全量芯片支持。

## 功能说明

向TensorDesc中设置Tensor的原始Format。 该Format是指原始网络模型的Format。

## 函数原型

```python
set_origin_format(format: Format) -> TensorDesc
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| format | 输入 | 原始数据格式，类型为Format。 |

## 返回值说明

\(TensorDesc\) 返回自身以支持链式调用。

## 约束说明

- 如果format不是Format类型，抛出TypeError。
