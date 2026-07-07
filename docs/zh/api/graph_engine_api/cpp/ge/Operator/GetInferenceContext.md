# GetInferenceContext

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

获取当前算子传递infershape推导所需要的关联信息，比如前面算子的shape和DataType信息。

## 函数原型

```c++
InferenceContextPtr GetInferenceContext() const
```

## 参数说明

无。

## 返回值说明

返回当前operator的推理上下文。

InferenceContextPtr是指向InferenceContext类的指针的别名：

```c++
using InferenceContextPtr = std::shared_ptr<InferenceContext>;
```

## 异常处理

无。

## 约束说明

无。
