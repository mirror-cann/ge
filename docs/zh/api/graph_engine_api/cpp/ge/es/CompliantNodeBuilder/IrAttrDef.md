# IrAttrDef

该结构体用于旧接口，包含 std::string 字段。新代码推荐使用ABI安全的[IrAttrDefV2](./IrAttrDefV2/IrAttrDefV2.md)。

IR属性定义结构体，定义如下：

```c++
struct IrAttrDef {
  std::string attr_name;
  IrAttrType ir_attr_type;
  std::string attr_data_type;  // see `kIrAttrTypesMap` in `operator.cc`
  AttrValue attr_default_value;
}
```

参数说明如下：

| 成员名 | 类型 | 说明 |
| --- | --- | --- |
| attr_name | std::string | 属性名。 |
| ir_attr_type | IrAttrType | 属性类型。 |
| attr_data_type | std::string | 取值为kIrAttrTypesMap中的key值：<br>const std::map<std::string, std::string> kIrAttrTypesMap = {<br>   {"Int", "VT_INT"},<br>   {"Float", "VT_FLOAT"},<br>   {"String", "VT_STRING"},<br>   {"Bool", "VT_BOOL"},<br>   {"Tensor", "VT_TENSOR"},<br>   {"NamedAttrs", "VT_NAMED_ATTRS"},<br>   {"ListInt", "VT_LIST_INT"},<br>   {"ListFloat", "VT_LIST_FLOAT"},<br>   {"ListString", "VT_LIST_STRING"},<br>   {"ListBool", "VT_LIST_BOOL"},<br>   {"ListTensor", "VT_LIST_TENSOR"},<br>   {"Bytes", "VT_BYTES"},<br>   {"ListListInt", "VT_LIST_LIST_INT"},<br>   {"ListNamedAttrs", "VT_LIST_NAMED_ATTRS"},<br>   {"Type", "VT_DATA_TYPE"},<br>   {"ListType", "VT_LIST_DATA_TYPE"},<br>} |
| attr_default_value | AttrValue | 属性默认值。 |
