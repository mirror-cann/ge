# BuildGraphAndReset

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

构建并返回图对象。

## 函数原型

```c++
std::unique_ptr<ge::Graph> BuildGraphAndReset()
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr\<ge::Graph> | 构建完成的图对象。 |

## 约束说明

无
