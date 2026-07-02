# GetTensor（FlowMsg数据类型）

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品/Atlas A2 训练系列产品：支持

## 函数功能

获取FlowMsg中的Tensor指针。

## 函数原型

```cpp
Tensor *GetTensor() const
```

## 参数说明

无

## 返回值

返回Tensor类型指针。

## 异常处理

无。

## 约束说明

只有消息类型为TENSOR\_DATA\_TYPE时，才能获取Tensor类型指针，否则返回NULL。
