# VERIFY\_FUNC\_REG

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/operator\_reg.h\>

## 功能说明

注册算子的Verify函数。

## 函数原型

```c++
VERIFY_FUNC_REG(op_name, x)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| op_name | 输入 | 算子类型。 |
| x | 输入 | Verify函数名，和[IMPLEMT_VERIFIER](IMPLEMT_VERIFIER.md)的Verify函数名保持一致。 |

## 返回值说明

无

## 约束说明

无
