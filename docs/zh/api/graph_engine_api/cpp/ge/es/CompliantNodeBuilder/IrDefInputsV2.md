# IrDefInputsV2

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

定义ABI安全的IR输入规范。

## 函数原型

```c++
CompliantNodeBuilder &IrDefInputsV2(const IrInputDefV2 *input_ir_def, size_t input_ir_def_num)
CompliantNodeBuilder &IrDefInputsV2(std::initializer_list<IrInputDefV2> input_ir_def)
CompliantNodeBuilder &IrDefInputsV2(const std::vector<IrInputDefV2> &input_ir_def)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| input_ir_def | 输入 | 输入IR定义数组，调用期间必须有效。 |
| input_ir_def_num | 输入 | 输入IR定义数量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | CompliantNodeBuilder & | 当前构建器对象的引用，支持链式调用。 |

## 约束说明

无
