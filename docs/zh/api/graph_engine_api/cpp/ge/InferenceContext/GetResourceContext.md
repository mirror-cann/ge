# GetResourceContext

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

通过资源标识key来获取对应的资源上下文对象。

## 函数原型

```c++
ResourceContext *GetResourceContext(const ge::AscendString &key)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| key | 输入 | 资源的唯一标识。由资源类算子infershape函数指定。 |

## 返回值说明

资源上下文对象。

基础定义如下，资源类算子可以基于此扩展。

```c++
struct ResourceContext {  virtual ~ResourceContext() {}};
```

用于保存资源相关信息，如shape、datatype等。

## 约束说明

若使用[Create](Create.md)接口创建InferenceContext时未传入resource context管理器指针，则该接口返回空指针，因此使用其返回值之前需要判空。
