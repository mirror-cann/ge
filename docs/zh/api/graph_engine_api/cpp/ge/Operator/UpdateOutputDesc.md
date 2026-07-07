# UpdateOutputDesc

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

根据算子Output名称更新Output的TensorDesc。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus UpdateOutputDesc(const std::string &name, const TensorDesc &tensor_desc)
graphStatus UpdateOutputDesc(const char_t *name, const TensorDesc &tensor_desc)
graphStatus UpdateOutputDesc(const uint32_t index, const TensorDesc &tensor_desc)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 算子Output名称。 |
| tensor_desc | 输入 | TensorDesc对象。 |
| index | 输入 | 算子Output的序号。 |

## 返回值说明

graphStatus类型：更新TensorDesc成功，返回GRAPH\_SUCCESS，否则，返回GRAPH\_FAILED。

## 异常处理

无。

## 约束说明

无。
