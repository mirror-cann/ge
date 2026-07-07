# set\_error\_message

## 产品支持情况

全量芯片支持。

## 功能说明

设置错误信息，供GE记录或上报。

## 函数原型

```python
set_error_message(message: str) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| message | 输入 | 待设置的错误信息，字符串类型。 |

## 返回值说明

无

## 约束说明

仅可在当前run调用栈内使用。

## 调用示例

```python
from ge.passes import FusionBasePass, PassContext


class MyPass(FusionBasePass):
    def run(self, graph, context: PassContext):
        # self._ok为用户自定义的图校验方法；校验失败时设置错误信息
        if not self._ok(graph):
            context.set_error_message("my pass: invariant violated")
            return False
        return True

```
