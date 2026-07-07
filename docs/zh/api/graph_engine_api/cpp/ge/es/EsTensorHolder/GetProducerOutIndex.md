# GetProducerOutIndex

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取输出索引。

## 函数原型

```c++
int32_t GetProducerOutIndex() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | int32_t | 输出索引。 |

## 约束说明

无

## 调用示例

```c++
  auto builder = std::make_unique<ge::EsGraphBuilder>("graph");
  auto tensor0 = builder->CreateInput(0);
  int32_t outputIndex = tensor0.GetProducerOutIndex(); // 获取输出tensor0的输出索引
```
