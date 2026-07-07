# attr\_scope

## 产品支持情况

全量芯片支持。

## 功能说明

属性作用域上下文管理器。

## 函数原型

```python
attr_scope(attr_maps)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_maps | 输入 | 属性映射字典。 |

## 返回值说明

无

## 约束说明

无

## 调用示例

```python
with attr_scope({"attr_name": attr_value}):
    tensor = builder.create_input(0)
```
