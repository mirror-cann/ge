# IrAttrDefV2构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

IrAttrDefV2构造函数和析构函数。

## 函数原型

```c++
IrAttrDefV2()
IrAttrDefV2(const char_t *attr_name, IrAttrType ir_attr_type, const char_t *attr_data_type, const AttrValue &attr_default_value)
~IrAttrDefV2()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr_name | 输入 | 属性名称，标识该属性在算子中的名称。 |
| ir_attr_type | 输入 | 属性类型，决定属性是必需还是可选。 |
| attr_data_type | 输入 | 属性数据类型，指定属性值的数据类型。 |
| attr_default_value | 输入 | 属性默认值，可选属性的默认取值。 |

## 返回值说明

构造并返回一个IrAttrDefV2对象。

## 约束说明

无
