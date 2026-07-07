# ToStreamGraph

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取根图和子图各自的流图AscendString，格式为DOT文件。

## 函数原型

```c++
const std::map<AscendString, AscendString> &ToStreamGraph() const
```

## 参数说明

无

## 返回值说明

返回Map，key为图名称，value为流图的DOT文件格式表示。

## 约束说明

无
