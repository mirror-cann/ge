# Run

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/pattern\_fusion\_pass.h\>
- 库文件：libge\_compiler.so

## 功能说明

执行Pass的主函数。

该函数将获取到PatternGraphs构造的pattern graph，用来构造pattern。

在目标Graph中逐一进行匹配，调用Requires函数对匹配结果做是否可被替换的校验，若为true，则进行进一步的替换；通过调用Replacement获取到替换的目标结构，将match result对应的子图结构替换为replacement子图。

注意: Run函数只处理当前图，若需要处理子图，由Pass调用者来负责。

## 函数原型

```c++
Status Run(GraphPtr &graph, CustomPassContext &pass_context) override
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 目标图。 |
| pass_context | 输入 | 上下文，可用于传递error msg等信息。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：执行成功<br>FAILED：执行失败 |

## 约束说明

无
