# Run

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/fusion\_base\_pass.h\>
- 库文件：libge\_compiler.so

## 功能说明

Pass执行主函数。

## 函数原型

```c++
virtual Status Run(GraphPtr &graph, CustomPassContext &pass_context) = 0
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 目标图。 |
| pass_context | 输入 | 自定义Pass上下文，可用于传递error msg等信息。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：执行成功<br>FAILED：执行失败 |

## 约束说明

无
