# remove\_subgraph

## 产品支持情况

全量芯片支持。

## 功能说明

根据名称移除子图。

## 函数原型

```python
remove_subgraph(name: str) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 子图名称，string类型。 |

## 返回值说明

无

## 约束说明

- 如果name不是字符串，抛出TypeError。
- 如果移除失败，抛出RuntimeError。
