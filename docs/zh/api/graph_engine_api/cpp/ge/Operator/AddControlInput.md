# AddControlInput

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

添加算子的控制边，控制边目前只是控制算子的执行顺序。

## 函数原型

```c++
Operator &AddControlInput(const Operator &src_oprt)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| src_oprt | 输入 | 控制边对应的源算子。 |

## 返回值说明

算子对象本身。

## 异常处理

无

## 约束说明

无
