# get\_error\_message

## 产品支持情况

全量芯片支持。

## 功能说明

返回由引擎或Pass设置的错误信息（未设置时返回空字符串）。

## 函数原型

```python
get_error_message() -> str
```

## 参数说明

无

## 返回值说明

返回错误信息字符串；未设置时返回空字符串。

## 约束说明

仅可在当前run调用栈内使用。

## 调用示例

```python
from ge.passes import FusionBasePass, PassContext


class MyPass(FusionBasePass):
    def run(self, graph, context: PassContext):
        # 在run()中读取错误信息
        msg = context.get_error_message()
        return True
```
