# InstanceOutputStorageShape

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置输出存储形状。

## 函数原型

```c++
CompliantNodeBuilder &InstanceOutputStorageShape(const char_t *name, const std::vector<int64_t> &shape)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 输出名称。 |
| shape | 输入 | 存储形状向量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | CompliantNodeBuilder & | 当前构建器对象的引用，支持链式调用。 |

## 约束说明

无
