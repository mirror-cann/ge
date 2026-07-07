# ClearChangedResourceKeys

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

一般由框架调用。

当变化了的资源触发重新推导之后，需要调用该接口清除inference\_context中保存的变化了的资源标识。

## 函数原型

```c++
void ClearChangedResourceKeys()
```

## 参数说明

无。

## 返回值说明

无。

## 约束说明

无。
