# UnregisterExternalAllocator

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

将用户基于Stream注册的Allocator销毁，适用于使用用户的内存池场景。

## 函数原型

```c++
Status UnregisterExternalAllocator(const void *const stream) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| stream | 输入 | 指定需要销毁的用户Allocator在哪个流上。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：设置成功。<br>FAILED：设置失败。 |

## 约束说明

- 用户销毁Allocator前，调用本接口取消注册。
- 待取消注册的Stream不存在，或多次调用本接口取消注册，本接口内部不做任何操作，返回成功。
