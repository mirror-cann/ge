# SetAttrValue

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/attr\_value.h\>
- 库文件：libgraph\_base.so

## 功能说明

设置属性的取值。泛型类，承载具体的属性类型。

## 函数原型

```c++
graphStatus SetAttrValue(const int64_t &attr_value) const
graphStatus SetAttrValue(const float32_t &attr_value) const
graphStatus SetAttrValue(const AscendString &attr_value) const
graphStatus SetAttrValue(const bool &attr_value) const
graphStatus SetAttrValue(const Tensor &attr_value) const
graphStatus SetAttrValue(const std::vector<int64_t> &attr_value) const
graphStatus SetAttrValue(const std::vector<float32_t> &attr_value) const
graphStatus SetAttrValue(const std::vector<AscendString> &attr_values) const
graphStatus SetAttrValue(const std::vector<bool> &attr_value) const
graphStatus SetAttrValue(const std::vector<Tensor> &attr_value) const
graphStatus SetAttrValue(const std::vector<std::vector<int64_t>> &attr_value) const
graphStatus SetAttrValue(const std::vector<ge::DataType> &attr_value) const
graphStatus SetAttrValue(const ge::DataType &attr_value) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_value | 输入 | 具体的属性取值。 |

## 返回值说明

graphStatus类型：成功，返回GRAPH\_SUCCESS， 否则，返回GRAPH\_FAILED。

## 异常处理

无

## 约束说明

无
