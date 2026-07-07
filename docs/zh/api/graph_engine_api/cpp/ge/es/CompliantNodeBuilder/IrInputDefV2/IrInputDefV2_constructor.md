# IrInputDefV2构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

IrInputDefV2构造函数和析构函数。

## 函数原型

```c++
IrInputDefV2()
IrInputDefV2(const char_t *name, IrInputType ir_input_type, const char_t *symbol_id)
~IrInputDefV2()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 节点的输入端口名称，用于标识该输入在算子中的位置。 |
| ir_input_type | 输入 | 定义输入的IR类型，决定输入是否必需、可选或动态。 |
| symbol_id | 输入 | 输入的符号标识，用于动态shape推导或符号化表达。 |

## 返回值说明

构造并返回一个IrInputDefV2对象。

## 约束说明

无
