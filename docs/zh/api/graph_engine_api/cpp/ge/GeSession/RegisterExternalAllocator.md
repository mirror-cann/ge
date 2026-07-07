# RegisterExternalAllocator

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

用户将自己的Allocator注册给GE，适用于使用用户的内存池场景。

此接口需要配合[LoadGraph](./LoadGraph.md)、[RunGraphWithStreamAsync](./RunGraphWithStreamAsync.md)接口使用，并且需要在LoadGraph接口调用前注册。

## 函数原型

```c++
Status RegisterExternalAllocator(const void *const stream, AllocatorPtr allocator) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| stream | 输入 | 指定Allocator注册在哪个Stream上。 |
| allocator | 输入 | 用户Allocator对象的智能指针。Allocator基于[《基础数据结构和接口》](https://hiascend.com/document/redirect/CannCommunitybasicopapi)中的"ge命名空间 > Allocator"派生。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：设置成功。<br>FAILED：设置失败。 |

## 约束说明

- 对于同一条流，多次调用本接口，以最后一次注册为准。
- 对于不同流，如果用户使用同一个Allocator，不可以多条流并发执行，在执行下一条Stream前，需要对上一Stream做流同步。
- 将Allocator中的内存释放给操作系统前，需要先调用接口“aclrtSynchronizeStream”执行流同步，确保Stream中的任务已执行完成。

    接口详细介绍请参见《Runtime运行时 API》中的“Stream管理”。
