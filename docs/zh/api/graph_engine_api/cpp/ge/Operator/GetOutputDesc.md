# GetOutputDesc

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

根据算子Output名称或Output索引获取算子Output的TensorDesc。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
TensorDesc GetOutputDesc(const std::string &name) const
TensorDesc GetOutputDescByName(const char_t *name) const
TensorDesc GetOutputDesc(uint32_t index) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 算子Output名称。<br>当无此算子Output名称时，返回TensorDesc默认构造的对象，其中，主要设置[DataType](https://hiascend.com/document/redirect/CannCommunitybasicopapi)为DT_FLOAT（表示float类型），[Format](https://hiascend.com/document/redirect/CannCommunitybasicopapi)为FORMAT_NCHW（表示NCHW）。 |
| index | 输入 | 算子Output索引。<br>当无此算子Output索引时，则返回TensorDesc默认构造的对象，其中，主要设置[DataType](https://hiascend.com/document/redirect/CannCommunitybasicopapi)为DT_FLOAT（表示float类型），[Format](https://hiascend.com/document/redirect/CannCommunitybasicopapi)为FORMAT_NCHW（表示NCHW）。 |

## 返回值说明

算子Output的。

## 异常处理

无。

## 约束说明

无。
