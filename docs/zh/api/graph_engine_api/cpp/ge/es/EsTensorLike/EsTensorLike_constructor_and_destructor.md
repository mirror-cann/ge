# EsTensorLike构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_like.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

EsTensorLike构造函数和析构函数。

## 函数原型

```c++
EsTensorLike(const EsTensorHolder &tensor)
EsTensorLike(const std::nullptr_t)
EsTensorLike(const int64_t value)
EsTensorLike(const float value)
EsTensorLike(const std::vector<int64_t> &values)
EsTensorLike(const std::vector<float> &values)
~EsTensorLike()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | EsTensorHolder对象。 |
| nullptr_t | 输入 | 空指针。 |
| value | 输入 | int64_t、float标量。 |
| values | 输入 | int64_t、float向量。 |

## 返回值说明

无

## 约束说明

无

## 调用示例

```c++
EsTensorHolder tensor = builder->CreateScalar(int64_t(1));
EsTensorLike tensor_like(tensor);
```
