# Create

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

在资源类算子推理的上下文中，创建资源算子的上下文对象。

## 函数原型

```c++
static std::unique_ptr<InferenceContext> Create(
void *resource_context_mgr = nullptr
)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| resource_context_mgr | 输入 | Resource Context管理器指针。<br>Session创建时候会初始化此指针，由InferShape框架自动传入，生命周期同Session。 |

## 返回值说明

资源类算子间传递的上下文对象。

## 异常处理

无。

## 约束说明

无。
