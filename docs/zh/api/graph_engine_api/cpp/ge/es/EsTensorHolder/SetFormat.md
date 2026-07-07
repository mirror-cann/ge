# SetFormat

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置TensorHolder的数据格式。

## 函数原型

```c++
EsTensorHolder &SetFormat(const ge::Format format)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| format | 输入 | 张量格式。详情请参见[Format](https://hiascend.com/document/redirect/CannCommunitybasicopapi)。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsTensorHolder & | 返回当前对象的引用，支持链式调用。 |

## 约束说明

无

## 调用示例

```c++
// 1. 创建图构建器（EsGraphBuilder）
EsGraphBuilder builder("graph_name");
// 2. 添加输入节点
EsTensorHolder data0 = builder.CreateInput(0).SetFormat(FORMAT_NCHW); // 设置输入节点输出tensor的格式类型
```
