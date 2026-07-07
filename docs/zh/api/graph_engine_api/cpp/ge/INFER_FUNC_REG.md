# INFER\_FUNC\_REG

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/operator\_reg.h\>

## 功能说明

注册算子的InferShape函数。

## 函数原型

```c++
INFER_FUNC_REG(op_name, x)
```

该函数内部会自动调用INFER\_VERIFY\_FUNC\(op\_name, x\)，INFER\_VERIFY\_FUNC函数中的op\_name为算子的类型，x为指向INFER\_FUNC\_REG（op\_name,x）中“x”的指针。

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| op_name | 输入 | 算子类型。 |
| x | 输入 | InferShape函数名，和[IMPLEMT_INFERFUNC](IMPLEMT_INFERFUNC.md)的InferShape函数名保持一致。 |

## 返回值说明

无

## 约束说明

无
