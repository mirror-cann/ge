# ELMTWISE\_INFER\_SHAPEANDTYPE

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/operator\_reg.h\>

## 功能说明

提供公共函数宏封装，供算子开发者开发InferShape函数。该函数基于输入的shape和dtype，设置输出的shape和dtype。

例如，输入shape为（1,2,3,4），dtype为float，则该宏会设置算子的输出shape为（1,2,3,4），输出dtype为float。

## 函数原型

```c++
ELMTWISE_INFER_SHAPEANDTYPE(in_name, out_name)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| in_name | 输入 | 算子输入。 |
| out_name | 输入 | 算子输出。 |

## 返回值说明

执行成功或失败。

## 约束说明

无

## 调用示例

```c++
COMMON_INFER_FUNC_REG(DiagD, ELMTWISE_INFER_SHAPEANDTYPE("assist", "y"));
```
