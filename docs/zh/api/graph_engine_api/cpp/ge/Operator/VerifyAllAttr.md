# VerifyAllAttr

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

根据disableCommonVerifier值，校验Operator中的属性是否有效，校验Operator的输入输出是否有效。

## 函数原型

```c++
graphStatus VerifyAllAttr(bool disable_common_verifier = false)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| disable_common_verifier | 输入 | 当false时，只校验属性有效性，当true时，增加校验Operator所有输入输出有效性。<br>默认值为false。 |

## 返回值说明

graphStatus类型：推导成功，返回GRAPH\_SUCCESS，否则，返回GRAPH\_FAILED。

## 异常处理

无。

## 约束说明

无。
