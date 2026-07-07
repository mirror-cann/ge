# GetOwnerBuilder

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_like.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取当前TensorLike对应Tensor的owner builder。

## 函数原型

```c++
EsCGraphBuilder *GetOwnerBuilder() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCGraphBuilder * | EsCGraphBuilder指针。 |

## 约束说明

无

## 调用示例

```c++
EsTensorLike tensor_like(tensor);
auto *owner_builder = tensor_like.GetOwnerBuilder();
```
