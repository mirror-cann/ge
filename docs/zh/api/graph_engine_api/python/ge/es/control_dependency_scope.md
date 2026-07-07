# control\_dependency\_scope

## 产品支持情况

全量芯片支持。

## 功能说明

控制依赖作用域上下文管理器。

## 函数原型

```python
control_dependency_scope(tensors: List[TensorHolder])
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| tensors | 输入 | 张量持有者列表。 |

## 返回值说明

无

## 约束说明

无

## 调用示例

```python
with control_dependency_scope([tensor1, tensor2]):
    tensor3 = builder.create_input(0)
```
