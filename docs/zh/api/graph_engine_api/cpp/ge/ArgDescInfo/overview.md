# 简介

ArgDescInfo用来描述Kernel输入参数Args地址信息的类，它对Args地址进行语义化表达，明确了每个地址的用途和相关信息。多个ArgDescInfo对象可以组成一个ArgsFormat，代码表达为std::vector<ArgDescInfo\>。

## 需要包含的头文件

```c++
#include <graph/arg_desc_info.h>
```

## Public成员函数

```c++
explicit ArgDescInfo(ArgDescType arg_type, int32_t ir_index = -1, bool is_folded = false)
static ArgDescInfo CreateCustomValue(uint64_t custom_value)
static ArgDescInfo CreateHiddenInput(HiddenInputSubType hidden_type)
ArgDescType GetType() const
uint64_t GetCustomValue() const
graphStatus SetCustomValue(uint64_t custom_value)
HiddenInputSubType GetHiddenInputSubType() const
graphStatus SetHiddenInputSubType(HiddenInputSubType hidden_type)
int32_t GetIrIndex() const
void SetIrIndex(int32_t ir_index)
bool IsFolded() const
void SetFolded(bool is_folded)
```
