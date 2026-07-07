# IsIrRepSupport

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

查询IR（Intermediate Representation）表达能力。

## 函数原型

```c
bool IsIrRepSupport(const char *rep)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| rep | 输入 | IR表达能力。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | bool | true：查询成功。<br>false：查询失败。 |

## 约束说明

无

## 调用示例

```c
#include "ge/ge_api_v2.h"
bool existingFeature = IsIrRepSupport("_inference_rule");
```
