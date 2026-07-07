# aclskScopeBegin

## 产品支持情况

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：不支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：不支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/c/super_kernel/aclskScopeBegin_res.md#id1 -->

## 头文件

\#include <super\_kernel/super\_kernel.h\>

## 功能说明

标记Super Kernel作用域的结束位置。调用该接口后，会在指定Stream上下发scope\_begin kernel及placeholder kernel，用于标识一组需要融合执行的算子操作的起始边界。

## 函数原型

```c
aclError aclskScopeBegin(const char* scopeName, aclrtStream stream)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| scopeName | 输入 | Super Kernel作用域的名称，用于唯一标识一个融合作用域。<br>若传入为nullptr，则标识不融合。 |
| stream | 输入 | 指定的acl运行时流，作用域标记kernel将在此流上执行。 |

## 返回值说明

返回0表示成功，返回其他值表示失败。

返回ACL\_ERROR\_INVALID\_PARAM表示参数无效（scopeName为空字符串数组，或长度超过255个字符）。

## 约束说明

- 与[aclskScopeEnd](aclskScopeEnd.md)成对使用，传入的scopeName与aclskScopeEnd一致。
- scope name名称不超过256B字节（注：scopeName入参类型是char，scopeName实际有效名称长度不超过255字节）。
- 全局支持的scopename（不同）数量不超过256个。
