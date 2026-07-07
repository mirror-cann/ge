# IrOutputDef

说明：该结构体用于旧接口，包含std::string字段。新代码推荐使用ABI安全的[IrOutputDefV2](./IrOutputDefV2/IrOutputDefV2.md)。

IR输出定义结构体，定义如下：

```c++
struct IrOutputDef {
  std::string name;
  IrOutputType ir_output_type;
  std::string symbol_id;
}
```

参数说明如下：

| 成员名 | 类型 | 说明 |
| --- | --- | --- |
| name | std::string | 输出名。 |
| ir_output_type | IrOutputType | 输出类型。 |
| symbol_id | std::string | 预留字段。 |
