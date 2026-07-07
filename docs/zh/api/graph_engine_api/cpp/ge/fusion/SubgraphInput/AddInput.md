# AddInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler.so

## 功能说明

为SubgraphInput添加一个边界输入。

## 函数原型

```c++
Status AddInput(const NodeIo &node_input)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node_input | 输入 | Node节点输入 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功。<br>FAILED：失败 。 |

## 约束说明

无
