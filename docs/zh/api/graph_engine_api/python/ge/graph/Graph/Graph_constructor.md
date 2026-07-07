# Graph构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

创建一个新的Graph对象。

## 函数原型

```python
Graph(name: Optional[str] = "graph")
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 图名称。<br>string类型，默认为graph。 |

## 返回值说明

创建的Graph对象。

## 约束说明

- 如果name不是字符串类型，抛出TypeError。
- 如果图创建失败，抛出RuntimeError。

## 调用示例

```python
graph = Graph("my_graph")
```
