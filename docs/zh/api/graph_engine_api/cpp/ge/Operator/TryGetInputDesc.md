# TryGetInputDesc

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

根据算子Input名称获取算子Input的TensorDesc。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus TryGetInputDesc(const std::string &name, TensorDesc &tensor_desc) const
graphStatus TryGetInputDesc(const char_t *name, TensorDesc &tensor_desc) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 算子的Input名。 |
| tensor_desc | 输出 | 返回算子端口的当前设置格式，为TensorDesc对象。 |

## 返回值说明

graphStatus类型：True，有此端口，获取TensorDesc成功；False，无此端口，出参为空，获取TensorDesc失败。

## 异常处理

| 异常场景 | 说明 |
| --- | --- |
| 无对应name输入 | 返回False。 |

## 约束说明

无。
