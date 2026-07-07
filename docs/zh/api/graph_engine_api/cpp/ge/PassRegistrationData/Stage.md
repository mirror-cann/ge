# Stage

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

设置自定义Pass执行阶段。

## 函数原型

```c++
PassRegistrationData &Stage(const CustomPassStage stage)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| stage | 输入 | 表示自定义Pass执行阶段。详情请参见[CustomPassStage](../CustomPassStage.md)。 |

## 返回值说明

返回PassRegistrationData对象。

## 约束说明

无
