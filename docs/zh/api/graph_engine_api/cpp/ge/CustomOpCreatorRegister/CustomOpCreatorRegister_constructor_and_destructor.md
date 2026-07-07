# CustomOpCreatorRegister构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/custom\_op.h\>
- 库文件：libgraph.so

## 功能说明

CustomOpCreatorRegister构造函数和析构函数。

## 函数原型

```c++
CustomOpCreatorRegister(const AscendString &operator_type, const BaseOpCreator &op_creator)
~CustomOpCreatorRegister() = default
```
