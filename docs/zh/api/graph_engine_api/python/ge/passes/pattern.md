# pattern

## 产品支持情况

全量芯片支持。

## 功能说明

标记PatternFusionPass的一个方法为表达式pattern声明的装饰器。

## 函数原型

```python
pattern(method: Callable[..., object]) -> Callable[..., object]
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| method | 输入 | PatternFusionPass子类中的实例方法，签名应为method(self, inputs)。 |

## 返回值说明

返回原方法对象，并在类定义阶段由PatternFusionPass收集为pattern。

## 约束说明

无
