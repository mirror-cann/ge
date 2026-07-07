# Stage

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/fusion\_pass\_reg.h\>
- 库文件：libge\_compiler.so

## 功能说明

融合Pass阶段定义。

注意：kAfterAssignLogicStream阶段不可注册普通融合Pass，此阶段不允许修改图结构，若误将Pass注册到此阶段，将被忽略。

## 函数原型

```c++
FusionPassRegistrationData &Stage(CustomPassStage stage)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| stage | 输入 | 表示自定义融合Pass执行阶段。详情请参见[CustomPassStage](../../CustomPassStage.md)。 |

## 返回值说明

返回FusionPassRegistrationData对象。

## 约束说明

无
