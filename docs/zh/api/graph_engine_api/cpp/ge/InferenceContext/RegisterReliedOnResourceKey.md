# RegisterReliedOnResourceKey

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

注册依赖的资源。

一般由读类型的算子调用，如stack pop。因读类型算子的shape依赖资源算子，调用该接口注册依赖的资源标识。

若资源算子shape变化可触发读类型算子的重新推导。

## 函数原型

```c++
graphStatus RegisterReliedOnResourceKey(const ge::AscendString &key)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| key | 输入 | 资源的唯一标识。 |

## 返回值说明

graphStatus类型：GRAPH\_SUCCESS，代表成功；GRAPH\_FAILED，代表失败。

## 约束说明

无。
