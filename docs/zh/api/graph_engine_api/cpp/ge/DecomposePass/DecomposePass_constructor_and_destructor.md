# DecomposePass构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/decompose\_pass.h\>
- 库文件：libge\_compiler

## 功能说明

DecomposePass构造函数和析构函数。

## 函数原型

```c++
explicit DecomposePass(const std::vector<AscendString> &op_types)
virtual ~DecomposePass() = default
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| op_types | 输入 | 待匹配的节点类型，支持传入多个。 |

## 返回值说明

无

## 约束说明

无
