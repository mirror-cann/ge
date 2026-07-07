# SetInputAttr

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

设置算子输入Tensor属性的属性值。

算子可以包括多个属性，初次设置值后，算子属性值的类型固定，算子属性值的类型包括：

- 整型：接受int64\_t、uint32\_t、int32\_t类型的整型值

    以int64\_t为例，使用:

    SetInputAttr\(const char\_t \*dst\_name, const char\_t \*name, int64\_t attr\_value\);

    SetInputAttr\(const int32\_t index, const char\_t \*name, int64\_t attr\_value\);

    设置属性值，以:

    GetInputAttr\(const int32\_t index, const char\_t \*name, int64\_t &attr\_value\) const;

    GetInputAttr\(const char\_t \*dst\_name, const char\_t \*name, int64\_t &attr\_value\) const

    取值时，用户需保证整型数据没有截断，同理针对int32\_t和uint32\_t混用时需要保证不被截断。

- 整型列表：接受std::vector<int64\_t\>、std::vector<int32\_t\>、std::vector<uint32\_t\>、std::initializer\_list<int64\_t\>&&表示的整型列表数据
- 浮点数：float32\_t
- 浮点数列表：std::vector<float32\_t\>
- 字符串：string
- 布尔：bool
- 布尔列表：std::vector<bool\>

## 函数原型

```c++
Operator &SetInputAttr(const int32_t index, const char_t *name, const char_t *attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const char_t *attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, const AscendString &attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const AscendString &attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, int64_t attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, int64_t attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, int32_t attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, int32_t attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, uint32_t attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, uint32_t attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, bool attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, bool attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, float32_t attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, float32_t attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, const std::vector<AscendString> &attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const std::vector<AscendString> &attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, const std::vector<int64_t> &attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const std::vector<int64_t> &attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, const std::vector<int32_t> &attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const std::vector<int32_t> &attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, const std::vector<uint32_t> &attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const std::vector<uint32_t> &attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, const std::vector<bool> &attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const std::vector<bool> &attr_value)
Operator &SetInputAttr(const int32_t index, const char_t *name, const std::vector<float32_t> &attr_value)
Operator &SetInputAttr(const char_t *dst_name, const char_t *name, const std::vector<float32_t> &attr_value)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 属性名称。 |
| index | 输入 | 输入索引。 |
| dst_name | 输入 | 输入边名称。 |
| attr_value | 输入 | 需设置的int64_t表示的整型类型属性值。 |
| attr_value | 输入 | 需设置的int32_t表示的整型类型属性值。 |
| attr_value | 输入 | 需设置的uint32_t表示的整型类型属性值。 |
| attr_value | 输入 | 需设置的vector<int64_t>表示的整型列表类型属性值。 |
| attr_value | 输入 | 需设置的vector<int32_t>表示的整型列表类型属性值。 |
| attr_value | 输入 | 需设置的vector<uint32_t>表示的整型列表类型属性值。 |
| attr_value | 输入 | 需设置的浮点类型的属性值。 |
| attr_value | 输入 | 需设置的浮点列表类型的属性值。 |
| attr_value | 输入 | 需设置的布尔类型的属性值。 |
| attr_value | 输入 | 需设置的布尔列表类型的属性值。 |
| attr_value | 输入 | 需设置的字符串类型的属性值。 |
| attr_value | 输入 | 需设置的字符串列表类型的属性值。 |

## 返回值说明

[Operator](Operator.md)对象本身。

## 异常处理

无。

## 约束说明

无。
