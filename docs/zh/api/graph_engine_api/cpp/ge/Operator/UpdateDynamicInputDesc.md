# UpdateDynamicInputDesc

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

根据name和index的组合更新算子动态Input的TensorDesc。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus UpdateDynamicInputDesc(const std::string &name, uint32_t index, const TensorDesc &tensor_desc)
graphStatus UpdateDynamicInputDesc(const char_t *name, uint32_t index, const TensorDesc &tensor_desc)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 算子动态Input的名称。 |
| index | 输入 | 算子动态Input编号，编号从0开始。 |
| tensor_desc | 输入 | TensorDesc对象。 |

## 返回值说明

graphStatus类型：更新动态Input成功，返回GRAPH\_SUCCESS， 否则，返回GRAPH\_FAILED。

## 异常处理

无。

## 约束说明

无。
