# AddResource

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

将传入的资源转移给EsCGraphBuilder进行管理。

## 函数原型

```c++
T *AddResource(std::unique_ptr<T> value, std::function<void(void*)> deleter = DefaultDeleter<T>)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| value | 输入 | 将被转移的资源。 |
| deleter | 输入 | 资源析构函数。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | T *（将被转移的资源类型） | 资源持有者指针。 |

## 约束说明

无
