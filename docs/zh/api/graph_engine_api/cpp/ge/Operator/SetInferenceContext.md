# SetInferenceContext

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

向当前算子传递infershape推导所需要的关联信息，比如前面算子的shape和DataType信息。

## 函数原型

```c++
void SetInferenceContext(const InferenceContextPtr &inference_context)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| inference_context | 输入 | 当前operator的推理上下文。<br>InferenceContextPtr是指向InferenceContext类的指针的别名：<br>using InferenceContextPtr = std::shared_ptr\<InferenceContext>; |

## 返回值说明

无。

## 异常处理

无。

## 约束说明

无。
