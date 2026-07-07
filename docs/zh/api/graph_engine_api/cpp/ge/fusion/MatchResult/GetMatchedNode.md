# GetMatchedNode

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/match\_result.h\>
- 库文件：libge\_compiler.so

## 功能说明

根据Pattern Node获取到匹配到的Target Node。

## 函数原型

```c++
Status GetMatchedNode(const GNode &pattern_node, GNode &matched_node) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| pattern_node | 输入 | Pattern Graph中的一个Node。 |
| matched_node | 输出 | 匹配到的Node。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：匹配成功。<br>FAILED：匹配失败 。 |

## 约束说明

无
