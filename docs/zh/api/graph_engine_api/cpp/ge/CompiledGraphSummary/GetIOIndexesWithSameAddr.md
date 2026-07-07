# GetIOIndexesWithSameAddr

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取编译后逻辑地址相同的Graph输入索引和输出索引对。

## 函数原型

```c++
Status GetIOIndexesWithSameAddr(std::vector<std::pair<uint32_t, uint32_t>> &io_indexes) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| io_indexes | 输出 | 输入/输出索引对。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | - SUCCESS：接口调用成功。<br>  - FAILED：接口调用失败。 |

## 约束说明

无
