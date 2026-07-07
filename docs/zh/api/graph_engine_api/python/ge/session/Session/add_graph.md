# add\_graph

## 功能说明

向会话添加图。

## 函数原型

```python
add_graph(graph_id: int, add_graph: Graph, options: dict = None) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| graph_id | 输入 | 图ID。 |
| add_graph | 输入 | 要添加的图对象。 |
| options | 输入 | 配置选项。 支持的配置项请参见[options参数说明](../../../../cpp/ge/options_params/options_parameters_description.md)中graph级别的参数。|

## 返回值说明

无

## 约束说明

- 如果graph\_id不是整数，抛出TypeError。
- 如果add\_graph不是Graph类型，抛出TypeError。
- 如果options不是字典类型，抛出TypeError。
- 如果添加失败，抛出RuntimeError。
- 相同对象的Graph调用此接口注册，会导致不同的Graph ID实际共享同一个Graph对象，导致后续操作相互影响而出错。
- 不同的Graph对象请不要使用相同的Graph ID来添加，该情况下，只保留第一次添加的Graph对象，后续的Graph对象不会添加成功。
