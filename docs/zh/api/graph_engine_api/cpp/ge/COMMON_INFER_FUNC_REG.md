# COMMON\_INFER\_FUNC\_REG

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/operator\_reg.h\>

## 功能说明

注册算子的InferShape函数。

与[INFER\_FUNC\_REG](INFER_FUNC_REG.md)的区别是，此函数注册的InferShape函数入参为Operator基类而非子类，此接口支持多算子共用同一个InferShape函数。

## 函数原型

```c++
COMMON_INFER_FUNC_REG(op_name, x)
```

该函数内部会自动调用COMMON\_INFER\_VERIFY\_FUNC\(x\)，COMMON\_INFER\_VERIFY\_FUNC\(x\)函数中的x为指向COMMON\_INFER\_FUNC\_REG\(op\_name, x\)中“x”的指针。

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| op_name | 输入 | 算子类型。 |
| x | 输入 | InferShape函数名，和[IMPLEMT_COMMON_INFERFUNC](IMPLEMT_COMMON_INFERFUNC.md)的InferShape函数名保持一致。 |

## 返回值说明

无

## 约束说明

无
