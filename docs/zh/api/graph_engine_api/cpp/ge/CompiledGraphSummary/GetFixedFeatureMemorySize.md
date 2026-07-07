# GetFixedFeatureMemorySize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取编译后固定Feature内存总大小（适用于静态shape图和动态shape图）。

## 函数原型

```c++
Status GetFixedFeatureMemorySize(size_t &size) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| size | 输出 | 常量内存大小。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | - SUCCESS：接口调用成功。<br>  - FAILED：接口调用失败。 |

## 约束说明

无
