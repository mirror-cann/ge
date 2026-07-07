# get\_pass\_name

## 产品支持情况

全量芯片支持。

## 功能说明

返回当前Pass名称。

## 函数原型

```python
get_pass_name() -> str
```

## 参数说明

无

## 返回值说明

返回当前Pass名称，类型为字符串。

## 约束说明

仅可在当前run调用栈（或其他引擎定义的同步回调）内使用，不要保存到self上供其他线程使用。

## 调用示例

```python
from ge.passes import FusionBasePass, PassContext


class MyPass(FusionBasePass):
    def run(self, graph, context: PassContext):
        # 在run()中获取当前Pass名称
        name = context.get_pass_name()
        return True
```

更多用法可参考[融合pass样例](https://gitcode.com/cann/ge/tree/master/examples/fusion_pass)。
