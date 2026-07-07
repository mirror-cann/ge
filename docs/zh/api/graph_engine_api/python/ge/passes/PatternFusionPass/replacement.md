# replacement

## 产品支持情况

全量芯片支持。

## 功能说明

生成替换子图。

## 函数原型

```python
replacement(self, match_result: MatchResult) -> Graph
```

表达式pattern也可使用以下写法：

```python
# 简单替换，不需要匹配详情
def replacement(self, inputs) -> TensorHolder: ...
# 需要读取捕获Tensor或节点属性
def replacement(self, inputs, match_result) -> TensorHolder: ...
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| self | 输入 | PatternFusionPass子类的Pass实例对象。用于访问Pass配置、调用方法。 |
| inputs | 输入 | PatternInputs类型的替换图输入集合，用于创建替换图的输入Tensor。 |
| match_result | 输入 | MatchResult类型的Pattern匹配结果，包含匹配到的节点和边信息。|

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| Graph | 返回替换后的子图，类型为ge.graph.Graph。 |

## 约束说明

表达式replacement\(self, inputs\)可返回TensorHolder或非空TensorHolder列表/元组，Python层会自动构造替换图；需要读取匹配详情时可增加match\_result参数。
