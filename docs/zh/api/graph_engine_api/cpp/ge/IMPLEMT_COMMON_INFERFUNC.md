# IMPLEMT\_COMMON\_INFERFUNC

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/operator\_reg.h\>

## 功能说明

封装算子的Common\_InferShape函数。

与[IMPLEMT\_INFERFUNC](IMPLEMT_INFERFUNC.md)的区别是，此函数自动生成的一个类型为Operator类的对象op，可直接调用[Operator](./Operator/overview.md)接口进行InferShape的实现。若InferShape方法具有通用性，可被多个算子的原型实现调用，可选择此接口实现。

## 函数原型

```c++
IMPLEMT_COMMON_INFERFUNC(func_name)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| func_name | 输入 | InferShape函数名，用户自定义。 |

## 返回值说明

无

## 约束说明

无
