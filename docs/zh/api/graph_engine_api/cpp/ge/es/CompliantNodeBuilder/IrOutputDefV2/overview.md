# 简介

ABI安全的IR输出定义类，用于定义节点的输出规范（输出名称、输出类型、符号标识）。

## 需要包含的头文件

```c++
#include "compliant_node_builder.h"
```

## public成员函数

```c++
IrOutputDefV2()
IrOutputDefV2(const char_t *name, IrOutputType ir_output_type, const char_t *symbol_id)
~IrOutputDefV2()
IrOutputDefV2(const IrOutputDefV2 &other)
IrOutputDefV2 &operator=(const IrOutputDefV2 &other)
IrOutputDefV2(IrOutputDefV2 &&other) noexcept
IrOutputDefV2 &operator=(IrOutputDefV2 &&other) noexcept

IrOutputDefV2 &Name(const char_t *name)
IrOutputDefV2 &OutputType(IrOutputType ir_output_type)
IrOutputDefV2 &SymbolId(const char_t *symbol_id)

const char_t *GetName() const
IrOutputType GetOutputType() const
const char_t *GetSymbolId() const
```
