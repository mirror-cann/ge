# GetUsrStreamLabel

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取当前逻辑流对应的用户流标签。

## 函数原型

```c++
AscendString GetUsrStreamLabel() const
```

## 参数说明

无

## 返回值说明

AscendString类型的用户流标签。如果该逻辑流不是用户指定的，则返回空字符串。

## 约束说明

无
