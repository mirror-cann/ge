# create\_inputs

## 产品支持情况

全量芯片支持。

## 功能说明

创建多个输入。

## 函数原型

```python
create_inputs(num: int, start_index: int = 0) -> List[TensorHolder]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| num | 输入 | 要创建的输入数量。 |
| start_index | 输入 | 输入起始索引，如果非 0 表示已创建其他输入。 |

## 返回值说明

\(List\[TensorHolder\]\)输入张量持有者列表。

## 约束说明

- 输入节点索引应从0开始连续递增。
- 如果参数类型不正确，抛出TypeError。
- 如果输入创建失败，抛出RuntimeError。
