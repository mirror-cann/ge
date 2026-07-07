# IrOutputDefV2构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

IrOutputDefV2构造函数和析构函数。

## 函数原型

```c++
IrOutputDefV2()
IrOutputDefV2(const char_t *name, IrOutputType ir_output_type, const char_t *symbol_id)
~IrOutputDefV2()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 输出端口名称，标识该输出在算子中的位置。 |
| ir_output_type | 输入 | 输出类型，决定输出是必需还是动态。 |
| symbol_id | 输入 | 符号标识，用于动态shape推导。 |

## 返回值说明

构造并返回一个IrOutputDefV2对象。

## 约束说明

无
