# SetNeedIteration

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

标记Graph是否需要循环执行。

## 函数原型

```c++
void SetNeedIteration(bool need_iteration)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| need_iteration | 输入 | 标记图是否需要循环执行。取值：<br><br>  - true：循环执行。<br>  - false：不循环执行。 |

## 返回值说明

无

## 约束说明

需要与npu\_runconfig/iterations\_per\_loop、npu\_runconfig/loop\_cond、npu\_runconfig/one、npu\_runconfig/zero等搭配使用，用户需要先构造带有npu\_runconfig/iterations\_per\_loop、npu\_runconfig/loop\_cond、npu\_runconfig/one、npu\_runconfig/zero名字的variable算子节点。
