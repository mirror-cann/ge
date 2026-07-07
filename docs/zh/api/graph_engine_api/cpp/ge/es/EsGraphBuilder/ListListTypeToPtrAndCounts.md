# ListListTypeToPtrAndCounts

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

将ListListType属性入参转换为包含List of Ptrs和List inner size的pair。

## 函数原型

```c++
template <typename T>
std::pair<std::vector<const T *>, std::vector<int64_t>> ListListTypeToPtrAndCounts(const std::vector<std::vector<T>> &list_list_type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| T | 输入 | List of List的类型。 |
| list_list_type | 输入 | ListListType属性入参。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::pair<std::vector<const T *>, std::vector<int64_t>> | 包含List of Ptrs和List inner size的pair。 |

## 约束说明

无
