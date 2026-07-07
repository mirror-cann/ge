# GetOption

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/op\_compile\_context.h\>
- 库文件：liblowering.so

## 功能说明

获取编译期图上下文中的配置选项值，通过option\_key查询对应的option value并输出，用于算子编译时获取全局配置参数（如编译优化选项等）。

## 函数原型

```c++
ge::graphStatus GetOption(const ge::AscendString &option_key, ge::AscendString &option) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| option_key | 输入 | 配置选项的key名，用于指定要查询的选项类型。 |
| option | 输出 | 配置选项的值，函数将查询结果写入此参数。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功<br>其他值：失败 |

## 约束说明

无
