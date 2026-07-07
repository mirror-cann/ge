# 简介

ABI安全的IR属性定义类，用于定义节点的属性规范（属性名称、属性类型、数据类型、默认值）。

## 需要包含的头文件

```c++
#include "compliant_node_builder.h"
```

## public成员函数

```c++
IrAttrDefV2()
IrAttrDefV2(const char_t *attr_name, IrAttrType ir_attr_type, const char_t *attr_data_type, const AttrValue &attr_default_value)
~IrAttrDefV2()
IrAttrDefV2(const IrAttrDefV2 &other)
IrAttrDefV2 &operator=(const IrAttrDefV2 &other)
IrAttrDefV2(IrAttrDefV2 &&other) noexcept
IrAttrDefV2 &operator=(IrAttrDefV2 &&other) noexcept

IrAttrDefV2 &AttrName(const char_t *attr_name)
IrAttrDefV2 &AttrType(IrAttrType ir_attr_type)
IrAttrDefV2 &AttrDataType(const char_t *attr_data_type)
IrAttrDefV2 &DefaultValue(const AttrValue &attr_default_value)

const char_t *GetAttrName() const
IrAttrType GetAttrType() const
const char_t *GetAttrDataType() const
const AttrValue &GetDefaultValue() const
```
