# GetOutputAttr

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

根据属性名称获取算子输出Tensor对应的属性值。

## 函数原型

```c++
graphStatus GetOutputAttr(const int32_t index, const char_t *name, AscendString &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, AscendString &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, int64_t &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, int64_t &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, int32_t &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, int32_t &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, uint32_t &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, uint32_t &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, bool &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, bool &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, float32_t &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, float32_t &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, std::vector<AscendString> &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, std::vector<AscendString> &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, std::vector<int64_t> &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, std::vector<int64_t> &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, std::vector<int32_t> &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, std::vector<int32_t> &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, std::vector<uint32_t> &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, std::vector<uint32_t> &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, std::vector<bool> &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, std::vector<bool> &attr_value) const
graphStatus GetOutputAttr(const int32_t index, const char_t *name, std::vector<float32_t> &attr_value) const
graphStatus GetOutputAttr(const char_t *dst_name, const char_t *name, std::vector<float32_t> &attr_value) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 属性名称。 |
| index | 输入 | 输出索引。 |
| dst_name | 输入 | 输出边名称。 |
| attr_value | 输出 | 获取到的int64_t表示的整型类型属性值。 |
| attr_value | 输出 | 获取到的int32_t表示的整型类型属性值。 |
| attr_value | 输出 | 获取到的uint32_t表示的整型类型属性值。 |
| attr_value | 输出 | 获取到的vector<int64_t>表示的整型列表类型属性值。 |
| attr_value | 输出 | 获取到的vector<int32_t>表示的整型列表类型属性值。 |
| attr_value | 输出 | 获取到的vector<uint32_t>表示的整型列表类型属性值。 |
| attr_value | 输出 | 获取到的浮点类型的属性值。 |
| attr_value | 输出 | 获取到的浮点列表类型的属性值。 |
| attr_value | 输出 | 获取到的布尔类型的属性值。 |
| attr_value | 输出 | 获取到的布尔列表类型的属性值。 |
| attr_value | 输出 | 获取到的字符串类型的属性值。 |
| attr_value | 输出 | 获取到的字符串列表类型的属性值。 |

## 返回值说明

graphStatus类型：找到对应属性，返回GRAPH\_SUCCESS，否则返回GRAPH\_FAILED。

## 异常处理

无。

## 约束说明

无。
