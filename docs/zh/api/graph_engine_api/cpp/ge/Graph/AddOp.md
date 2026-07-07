# AddOp

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

用户算子缓存接口，通过此接口可以将不带连接关系的算子缓存在图中，用于查询和对象获取。

## 函数原型

```c++
graphStatus AddOp(const ge::Operator &op)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| op | 输入 | 需增加的算子。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | SUCCESS：操作成功<br>FAILED：操作失败 |

## 约束说明

此接口为非必须接口。主要用于用户在多个函数间切换时，operator对象可能因为是局部变量而无法被多个函数获取，此时operator可以注册在graph中，通过graph获取先前创建的operator对象，但注册到graph后，此operator对象除非graph销毁，否则一直存在。
