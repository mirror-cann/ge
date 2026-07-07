# Pattern构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern.h\>
- 库文件：libge\_compiler.so

## 功能说明

使用Graph定义匹配pattern。

## 函数原型

```c++
explicit Pattern(Graph &&pattern_graph)
~Pattern()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| pattern_graph | 输入 | Pattern Graph定义。构造完Pattern后，其生命周期归Pattern对象管理。 |

## 返回值说明

无

## 约束说明

无
