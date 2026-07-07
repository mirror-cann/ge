# run

## 产品支持情况

全量芯片支持。

## 功能说明

Pass执行入口，由引擎在对应编译阶段调用，对计算图执行优化逻辑。

## 函数原型

```python
run(graph: Graph, context: PassContext) -> StatusLike
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 待优化的计算图对象，类型为ge.graph.Graph。 |
| context | 输入 | Pass执行上下文，类型为PassContext，提供当前编译环境信息。 |

## 返回值说明

返回None、bool或int。返回None或真值表示执行成功，返回假值False或0表示执行失败。

## 约束说明

子类必须实现该方法。
