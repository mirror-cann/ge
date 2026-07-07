# GetCapturedTensors

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取Pattern中捕获的所有Tensor，vector中顺序为捕获顺序。

## 函数原型

```c++
Status GetCapturedTensors(std::vector<NodeIo> &node_outputs) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node_outputs | 输入 | 捕获Pattern中的所有Tensor。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：获取成功。<br>FAILED：获取失败 。 |

## 约束说明

无
