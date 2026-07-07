# IrInputDef

该结构体用于旧接口，包含std::string字段。新代码推荐使用ABI安全的[IrInputDefV2](./IrInputDefV2/IrInputDefV2.md)。

IR输入定义结构体，定义如下：

```c++
struct IrInputDef {
  std::string name;
  IrInputType ir_input_type;
  std::string symbol_id;
}
```

参数说明如下：

| 成员名 | 类型 | 说明 |
| --- | --- | --- |
| name | std::string | 输入名。 |
| ir_input_type | IrInputType | 输入类型。 |
| symbol_id | std::string | 预留字段。 |
