# 简介

ABI安全的IR输入定义类。

## 需要包含的头文件

```c++
#include "compliant_node_builder.h"
```

## public成员函数

```c++
IrInputDefV2()
IrInputDefV2(const char_t *name, IrInputType ir_input_type, const char_t *symbol_id)
~IrInputDefV2()
IrInputDefV2(const IrInputDefV2 &other)
IrInputDefV2 &operator=(const IrInputDefV2 &other)
IrInputDefV2(IrInputDefV2 &&other) noexcept
IrInputDefV2 &operator=(IrInputDefV2 &&other) noexcept
IrInputDefV2 &Name(const char_t *name)
IrInputDefV2 &InputType(IrInputType ir_input_type)
IrInputDefV2 &SymbolId(const char_t *symbol_id)
const char_t *GetName() const
IrInputType GetInputType() const
const char_t *GetSymbolId() const
const char_t *GetName() const
IrInputType GetInputType() const
const char_t *GetSymbolId() const
```
