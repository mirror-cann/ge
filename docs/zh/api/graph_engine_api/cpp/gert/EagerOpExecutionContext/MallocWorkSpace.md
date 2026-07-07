# MallocWorkSpace

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/eager\_op\_execution\_context.h\>
- 库文件：liblowering.so

## 功能说明

分配Workspace内存，Placement为Device。

内存由context构造方管理，接口调用者不需要主动释放。

## 函数原型

```c++
void *MallocWorkSpace(size_t size)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| size | 输入 | 内存大小，单位为字节。 |

## 返回值说明

地址指针，异常时返回空指针。

## 约束说明

无
