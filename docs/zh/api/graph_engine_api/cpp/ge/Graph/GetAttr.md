# GetAttr

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

获取Graph的属性，泛型属性接口，属性的类型为attr\_value。

## 函数原型

```c++
graphStatus GetAttr(const AscendString &name, AttrValue &attr_value) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输出 | 返回Graph的属性名称。 |
| attr_value | 输出 | 返回Graph的属性值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无

## 调用示例

```c++
AttrValue av_get;
graph.GetAttr("int_attr", av_get);
int64_t int_attr_get{};
av_get.GetAttrValue(int_attr_get);
ASSERT_EQ(int_attr_get, 100);
```
