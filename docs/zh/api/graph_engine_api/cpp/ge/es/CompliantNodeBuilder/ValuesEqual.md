# ValuesEqual

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

比较两个值是否相等。

## 函数原型

```c++
template <typename T>
bool ValuesEqual(const T &a, const T &b)
bool ValuesEqual(const std::vector<T> &a, const std::vector<T> &b)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| a | 输入 | 第一个值。 |
| b | 输入 | 第二个值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | bool | 如果相等返回true，否则返回false。 |

## 约束说明

无
