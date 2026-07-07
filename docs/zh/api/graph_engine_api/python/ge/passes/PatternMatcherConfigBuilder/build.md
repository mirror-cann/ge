# build

## 产品支持情况

全量芯片支持。

## 功能说明

构造并返回PatternMatcherConfig实例。

## 函数原型

```python
build() -> PatternMatcherConfig
```

## 参数说明

无

## 返回值说明

返回构造好的PatternMatcherConfig实例。

## 约束说明

若C++侧构建失败，将抛出RuntimeError。

## 调用示例

```python
from ge.passes import PatternFusionPass, PatternMatcherConfigBuilder


class TestPass(PatternFusionPass):
    def __init__(self):
        super().__init__(
            PatternMatcherConfigBuilder()
            .enable_const_value_match()
            .enable_ir_attr_match()
            .build()
        )
```
