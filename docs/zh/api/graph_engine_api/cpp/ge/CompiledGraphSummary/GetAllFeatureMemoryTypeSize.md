# GetAllFeatureMemoryTypeSize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取编译后所有内存类型的Feature内存大小，当前仅包含Fixed的内存（适用于静态shape图和动态shape图）。

## 函数原型

```c++
std::vector<FeatureMemoryPtr> GetAllFeatureMemoryTypeSize() const
```

## 参数说明

无

## 返回值说明

图编译结果中[FeatureMemory](../FeatureMemory/FeatureMemory.md)的shared\_ptr。

## 约束说明

无
