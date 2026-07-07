# GenerateNodeName

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

生成节点名称。

## 函数原型

```c++
ge::AscendString GenerateNodeName(const ge::char_t *node_type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node_type | 输入 | 节点类型。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | ge::AscendString | 生成的节点名称字符串。 |

## 约束说明

无
