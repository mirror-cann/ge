# GetReliedOnResourceKeys

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

一般由框架调用。

在结束读类型算子的推导后，可以调用该接口获取依赖的资源标识。

## 函数原型

```c++
const std::set<ge::AscendString>& GetReliedOnResourceKeys() const
```

## 参数说明

无。

## 返回值说明

依赖的资源标识集合。

## 约束说明

无。
