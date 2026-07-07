# AppendNodeMatchPair

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/match\_result.h\>
- 库文件：libge\_compiler.so

## 功能说明

增加一个匹配对。

## 函数原型

```c++
Status AppendNodeMatchPair(const NodeIo &pattern_node_out,const NodeIo &matched_node_out)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| pattern_node_out | 输入 | Pattern Graph中一个Node的某个输出。 |
| matched_node_out | 输入 | 在Target Graph中对应匹配到Node的输出。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：增加成功。<br>FAILED：增加失败 。 |

## 约束说明

无
