# GetStreamId

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

获取节点上当前的Stream ID，其结果表示经过内置流分配算法以后该节点被分配的Stream ID。

## 函数原型

```c++
int64_t GetStreamId(const GNode &node) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 图上节点。 |

## 返回值说明

节点所属的Stream ID。

## 约束说明

无
