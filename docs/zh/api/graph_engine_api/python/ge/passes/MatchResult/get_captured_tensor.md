# get\_captured\_tensor

## 产品支持情况

全量芯片支持。

## 功能说明

按capture\_tensor顺序返回第capture\_index个捕获的Tensor。

## 函数原型

```python
get_captured_tensor(capture_index: int) -> NodeIo
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| capture_index | 输入 | 捕获序号，需与capture_tensor的调用顺序一致。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| NodeIo | 返回对应的捕获Tensor，包含node与index属性 |

## 约束说明

capture\_index必须与capture\_tensor的顺序一致，否则C++层会失败并抛出异常。

## 调用示例

```python
# match_result为replacement(self, match_result)或
# meet_requirements(self, match_result)的入参
nodes = match_result.get_matched_nodes()
tensor0 = match_result.get_captured_tensor(0)  #NodeIo
n0, idx0 = tensor0.node, tensor0.index
```
