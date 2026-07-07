# IrDefOutputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

定义IR输出规范。

说明：该接口已废弃，推荐使用ABI安全的[IrDefOutputsV2](IrDefOutputsV2.md)接口。

## 函数原型

```c++
CompliantNodeBuilder &IrDefOutputs(std::vector<IrOutputDef> output_ir_def)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| output_ir_def | 输入 | 输出IR定义向量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | CompliantNodeBuilder & | 当前构建器对象的引用，支持链式调用。 |

## 约束说明

无
