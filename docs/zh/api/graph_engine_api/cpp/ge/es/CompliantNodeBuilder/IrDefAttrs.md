# IrDefAttrs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

定义IR属性规范。

说明：该接口已废弃，推荐使用ABI安全的[IrDefAttrsV2](IrDefAttrsV2.md)接口。

## 函数原型

```c++
CompliantNodeBuilder &IrDefAttrs(std::vector<IrAttrDef> attr_ir_def)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr_ir_def | 输入 | 属性IR定义向量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | CompliantNodeBuilder & | 当前构建器对象的引用，支持链式调用。 |

## 约束说明

无
