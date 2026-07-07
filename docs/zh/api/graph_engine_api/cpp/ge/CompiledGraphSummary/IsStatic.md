# IsStatic

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

返回graph是否是静态编译的，只有在静态编译的情况下，才可以使用后续的获取Summary信息的接口。

GetFixedFeatureMemorySize/GetAllFeatureMemoryTypeSize接口除外。

## 函数原型

```c++
bool IsStatic() const
```

## 参数说明

无

## 返回值说明

如果为静态，则返回true，否则返回false。

## 约束说明

无
