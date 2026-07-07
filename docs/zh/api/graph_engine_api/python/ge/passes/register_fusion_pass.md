# register\_fusion\_pass

## 产品支持情况

全量芯片支持。

## 功能说明

注册融合Pass的类装饰器，用于将FusionBasePass或PatternFusionPass子类注册到GE编译流程中。

## 函数原型

```python
register_fusion_pass(*, name: str, stage: PassStage, kind: Optional[str] = None) -> callable
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | Pass名称，字符串类型，必须唯一，不可与已注册的Pass名称重复。 |
| stage | 输入 | Pass执行阶段，类型为PassStage枚举。 |
| kind | 输入 | Pass类型标识，可选参数。若不指定，当被装饰类为PatternFusionPass子类时自动设为"pattern_fusion"，否则设为"fusion_base"。 |

## 返回值说明

返回类装饰器函数，被装饰的类会被注册到Pass注册表中，并附加\_\_ge\_pass\_descriptor\_\_ 属性。

## 约束说明

无
