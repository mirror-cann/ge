# GetPlatformInfos

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/op\_compile\_context.h\>
- 库文件：liblowering.so

## 功能说明

获取编译期的平台硬件信息和可选补充信息。

## 函数原型

```c++
ge::graphStatus GetPlatformInfos(fe::PlatFormInfos &platform_info, fe::OptionalInfos& optional_infos) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| platform_info | 输出 | 用于存储平台硬件信息，包含SoC版本、芯片规格等。 |
| optional_infos | 输出 | 用户存储可选补充信息。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功<br>其他值：失败 |

## 约束说明

无
