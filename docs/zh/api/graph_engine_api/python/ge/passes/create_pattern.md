# create\_pattern

## 产品支持情况

全量芯片支持。

## 功能说明

将传入的模式图包装为原生Pattern对象，用于在目标图中匹配子图。

## 函数原型

```python
create_pattern(graph: Graph) -> Pattern
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 模式图，类型为ge.graph.Graph。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| Pattern | 返回构建好的原生Pattern对象。 |

## 约束说明

无
