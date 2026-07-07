# get\_option\_value

## 产品支持情况

全量芯片支持。

## 功能说明

返回key对应的编译选项字符串值。

## 函数原型

```python
get_option_value(key: str) -> str
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| key | 输入 | 编译选项键名，字符串类型。 |

## 返回值说明

返回key对应的编译选项字符串值。

## 约束说明

- key必须存在且可读，否则抛出RuntimeError。
- 仅可在当前run调用栈内使用。

## 调用示例

```python
from ge.passes import FusionBasePass, PassContext


class MyPass(FusionBasePass):
    def run(self, graph, context: PassContext):
        # 在run()中读取编译选项
        opt = context.get_option_value("some.option.key")
        return True
```

更多用法可参考[融合pass样例](https://gitcode.com/cann/ge/tree/master/examples/fusion_pass)。
