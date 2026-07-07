# ToTensorHolder

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_like.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

将当前EsTensorLike转换为EsTensorHolder。

## 函数原型

```c++
EsTensorHolder ToTensorHolder(EsCGraphBuilder *graph) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 所属的图构建器。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsTensorHolder | 转换后的EsTensorHolder对象。 |

## 约束说明

无

## 调用示例

```c++
EsTensorLike tensor_like(tensor);
auto result = tensor_like.ToTensorHolder(c_builder);
```
