# add\_output

## 产品支持情况

全量芯片支持。

## 功能说明

将第index个边界输出绑定到SubgraphOutput。

## 函数原型

```python
add_output(index: int, output: SubgraphOutput) -> int
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 逻辑边界槽位序号，需与替换图输出顺序对齐。 |
| output | 输入 | 边界输出，类型为SubgraphOutput。 |

## 返回值说明

返回内部状态。

## 约束说明

无
