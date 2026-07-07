# CustomAllocateStreamPassFn

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

注册自定义的逻辑流分配Pass执行函数。

关于接口的详细使用方法请参见编程指南 \> 自定义Pass开发 \> 使用自定义逻辑流分配Pass定制并发。

## 函数原型

```c++
PassRegistrationData &CustomAllocateStreamPassFn(const CustomAllocateStreamPassFunc &allocate_stream_pass_fn)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| allocate_stream_pass_fn | 输入 | 自定义的逻辑流分配函数。详情请参见[回调函数CustomAllocateStreamPassFunc](#回调函数customallocatestreampassfunc)。 |

## 返回值说明

返回自身对象的引用。

## 约束说明

无。

## 回调函数CustomAllocateStreamPassFunc

用户自定义并实现CustomAllocateStreamPassFunc类函数，即自定义的逻辑流分配函数。

```c++
Status CustomAllocateStreamPassFunc(const ConstGraphPtr &graph, StreamPassContext &stream_context)
```

**表 1**  参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 待分配逻辑流的图 |
| stream_context | 输入 | 逻辑流分配上下文，可通过该上下文申请新stream id，设置节点的stream id等。 |
| - | 输出 | - SUCCESS：成功。<br>  - 其他值：失败。 |
