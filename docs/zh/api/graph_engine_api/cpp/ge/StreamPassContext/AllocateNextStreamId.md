# AllocateNextStreamId

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

申请新的Stream ID，当希望申请新逻辑流的时候，需要调用该接口。

## 函数原型

```c++
int64_t AllocateNextStreamId()
```

## 参数说明

无

## 返回值说明

新Stream ID。

## 约束说明

无
