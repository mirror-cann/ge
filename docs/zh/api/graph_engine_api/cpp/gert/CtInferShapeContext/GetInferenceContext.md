# GetInferenceContext

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/ct\_infer\_shape\_context.h\>

## 功能说明

获取InferenceContext指针。

## 函数原型

```c++
ge::InferenceContext *GetInferenceContext() const
```

## 参数说明

无

## 返回值说明

输出InferenceContext指针。

关于InferenceContext类型的定义，请参见[InferenceContext](../../ge/InferenceContext/overview.md)。

## 约束说明

无

## 调用示例

```c++
ge::graphStatus InferShapeForXXX(CtInferShapeContext *context) {
  const auto &read_inference_context = ct_context->GetInferenceContext();
  // ...
}
```
