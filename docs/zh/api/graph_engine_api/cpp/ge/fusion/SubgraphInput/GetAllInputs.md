# GetAllInputs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取SubgraphInput对应的所有边界输入。

## 函数原型

```c++
std::vector<NodeIo> GetAllInputs() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::vector\<NodeIo> | NodeIo表示一个Node的某个输入索引。 |

## 约束说明

无
