# add\_input

## 产品支持情况

全量芯片支持。

## 功能说明

将第index个边界输入绑定到SubgraphInput。

## 函数原型

```python
add_input(index: int, input: SubgraphInput) -> int
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 逻辑边界槽位序号，需与替换图输入顺序对齐。 |
| input | 输入 | 边界输入，类型为SubgraphInput。 |

## 返回值说明

返回内部状态。

## 约束说明

无
