# GetCapturedTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/match\_result.h\>
- 库文件：libge\_compiler.so

## 功能说明

根据Pattern中捕获的索引，获取匹配中对应的Node output。

## 函数原型

```c++
Status GetCapturedTensor(size_t capture_idx, NodeIo &node_output) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| capture_idx | 输入 | 捕获的索引ID。 |
| node_output | 输出 | 配到的Tensor。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功。<br>FAILED：失败 。 |

## 约束说明

无
