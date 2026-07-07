# GetOutputShapes

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

该接口只适用于静态Shape图，获取整个编译图的输出Shape，输出Shape可用于计算outputs所占内存大小；对于动态多档位图，获取当前最大档位的输出Shape。

## 函数原型

```c++
Status GetOutputShapes(std::vector<ge::Shape> &shapes) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| shapes | 输出 | ge::Shape类型的向量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | - SUCCESS：接口调用成功。<br>  - FAILED：接口调用失败。 |

## 约束说明

无
