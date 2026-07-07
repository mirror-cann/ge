# LogicalStreamAllocationInfo构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

LogicalStreamAllocationInfo构造函数和析构函数。

## 函数原型

```c++
LogicalStreamAllocationInfo()
~LogicalStreamAllocationInfo()
LogicalStreamAllocationInfo(const LogicalStreamAllocationInfo &stream_info)
LogicalStreamAllocationInfo &operator=(const LogicalStreamAllocationInfo &stream_info)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| stream_info | 输入 | 流信息。 |

## 返回值说明

无

## 约束说明

无
