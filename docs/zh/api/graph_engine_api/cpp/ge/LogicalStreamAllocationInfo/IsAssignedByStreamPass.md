# IsAssignedByStreamPass

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

当前逻辑流是否由用户注册的流分配Pass分配。

## 函数原型

```c++
bool IsAssignedByStreamPass() const
```

## 参数说明

无

## 返回值说明

bool类型，true或false。

- true：由用户注册的流分配Pass指定。
- false：来自GE内部分流或者用户流标签指定的流。

## 约束说明

无
