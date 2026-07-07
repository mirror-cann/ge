# INFER\_FORMAT\_FUNC\_REG

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/operator\_reg.h\>

## 功能说明

注册算子的InferFormat实现。

GE会在整图的Shape与Dtype推导前后分别调用一次整图的InferFormat，过程中会分别调用各个算子的InferFormat函数。如果算子没有注册InferFormat函数，GE将使用默认的推导函数，即输出的Format等于输入的Format。

## 函数原型

```c++
#define INFER_FORMAT_FUNC_REG(op_name, x) \
__INFER_FORMAT_FUNC_REG_IMPL__(op_name, INFER_FORMAT_FUNC(op_name, x), __COUNTER__)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| op_name | 输入 | 算子类型。 |
| x | 输入 | InferFormat函数名，使用IMPLEMT_INFERFORMAT_FUNC中的func_name。 |

## 返回值说明

无

## 约束说明

无

## 调用示例和相关API

```c++
INFER_FORMAT_FUNC_REG(Transpose, TransposeInferFormat);
```
