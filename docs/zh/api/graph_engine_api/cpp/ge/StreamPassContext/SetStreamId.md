# SetStreamId

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

设置某节点的Stream ID，拥有相同Stream ID的节点将会在同一条流上依次执行。

## 函数原型

```c++
graphStatus SetStreamId(const GNode &node, int64_t stream_id)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 图上节点。 |
| stream_id | 输入 | 待设置的Stream ID：若为已申请的stream id，直接设置即可；若需要新申请Stream ID，请先调用[AllocateNextStreamId](AllocateNextStreamId.md)接口申请，否则若Stream ID超出当前图上最大的Stream ID接口将返回失败。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：设置成功。<br>FAILED：设置失败。 |

## 约束说明

无
