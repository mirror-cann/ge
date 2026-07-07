# FindOpByName

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

基于算子名称，获取缓存在Graph中的op对象。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus FindOpByName(const std::string &name, ge::Operator &op) const
graphStatus FindOpByName(const char_t *name, ge::Operator &op) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 需要获取的算子名称。 |
| op | 输出 | 返回用户所需要的op对象。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | SUCCESS：成功获取算子实例。<br>FAILED：此名字没有在Graph中注册op对象。 |

## 约束说明

此接口为非必须接口，与[AddOp](AddOp.md)搭配使用。
