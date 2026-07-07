# register\_decompose\_pass

## 产品支持情况

全量芯片支持。

## 功能说明

将被装饰的DecomposePass子类注册到Pass注册表中，同时将op\_types设置为类的属性。

## 函数原型

```python
register_decompose_pass(*, name: str, stage: PassStage, op_types: Iterable[str]) -> callable
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | Pass名称，字符串类型，必须唯一，不可与已注册的Pass名称重复。 |
| stage | 输入 | Pass执行阶段，类型为PassStage枚举。 |
| op_types | 输入 | 需要分解的算子类型列表，类型为字符串的可迭代对象，不可为空，且每个元素必须为非空字符串。 |

## 返回值说明

返回类装饰器函数，被装饰的类会被注册到Pass注册表中，同时将op\_types设置为类的属性。

## 约束说明

无
