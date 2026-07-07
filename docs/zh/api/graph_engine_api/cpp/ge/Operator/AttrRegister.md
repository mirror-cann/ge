# AttrRegister

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

泛型属性注册接口。

## 函数原型

```c++
void AttrRegister(const char_t *name, const AttrValue &attr_value)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 属性名。 |
| attr_value | 输入 | 属性取值。 |

## 返回值说明

graphStatus类型：成功，返回GRAPH\_SUCCESS，否则，返回GRAPH\_FAILED。

## 异常处理

无

## 约束说明

无。
