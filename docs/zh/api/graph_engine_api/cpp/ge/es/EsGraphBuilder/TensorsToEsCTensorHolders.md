# TensorsToEsCTensorHolders

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

将EsTensorHolder向量转换为EsCTensorHolder指针向量。

## 函数原型

```c++
inline std::vector<EsCTensorHolder *> TensorsToEsCTensorHolders(const std::vector<EsTensorHolder> &tensors)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensors | 输入 | EsTensorHolder向量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::vector<EsCTensorHolder *> | 返回EsCTensorHolder指针向量。 |

## 约束说明

无
