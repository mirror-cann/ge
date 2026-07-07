# GetProducer

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取产生该张量的图节点。

## 函数原型

```c++
GNode *GetProducer() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | GNode * | 返回图节点指针。 |

## 约束说明

无

## 调用示例

```c++
  auto builder = std::make_unique<ge::EsGraphBuilder>("graph");
  auto tensor0 = builder->CreateInput(0);
  GNode *data0 = tensor0.GetProducer(); // 获取输入tensor0的节点data0
```
