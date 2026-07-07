# SetInputAttr

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

设置Graph的输入属性，泛型属性接口，属性类型为attr\_value。

## 函数原型

```c++
graphStatus SetInputAttr(const AscendString &name, uint32_t input_index, const AttrValue &attr_value)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 输入属性名。 |
| input_index | 输入 | 输入节点索引。 |
| attr_value | 输入 | 输入的属性值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
