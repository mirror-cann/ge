# @pattern

## 产品支持情况

全量芯片支持。

## 功能说明

使用Python表达式定义一个模式。一个@pattern方法对应一个pattern，多个pattern可声明多个@pattern方法。

## 函数原型

```python
@pattern
def add_zero(self, inputs):
    return inputs[0] + 0
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| inputs | 输入 | 表达式pattern输入集合。inputs[i]表示第i个图输入；inputs[:N]用于显式声明连续的多个图输入。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| TensorHolder | 返回单输出pattern表达式。 |
| list[TensorHolder] / tuple[TensorHolder, ...] | 返回一个多输出pattern；该列表或元组不表示多个pattern。 |

## 约束说明

- @pattern方法只负责声明pattern，不接收match\_result。
- 多pattern pass通过多个@pattern方法声明，不通过一个方法返回多个Pattern/Graph。
- inputs的输入数量未知，因此不能直接迭代；多输入场景请使用inputs\[:N\]。
- Python层会自动创建GraphBuilder、图输入、图输出，并自动capture已访问的inputs和@pattern返回的pattern输出。
- 自动capture顺序固定为：先按输入序号capture已访问的inputs，再按return结构顺序capture pattern输出。同一个Tensor同时作为输入和输出时，会按这两个角色各capture一次。

## 调用示例

```python
from ge.passes import PatternFusionPass, pattern


class AlgebraicPass(PatternFusionPass):
    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0

    @pattern
    def mul_one(self, inputs):
        return inputs[0] * 1

    def replacement(self, inputs):
        return inputs[0]
```

更多用法可参考[融合pass样例](https://gitcode.com/cann/ge/tree/master/examples/fusion_pass)。
