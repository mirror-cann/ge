# aclgrphProfFinalize

## 产品支持情况

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphProfFinalize_res.md#id1 -->

## 头文件/库文件

- 头文件：\#include <ge/ge\_prof.h\>
- 库文件：libmsprofiler.so

## 功能说明

结束Profiling。

## 函数原型

```c++
Status aclgrphProfFinalize()
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>其他：失败 |

## 约束说明

该接口需和[aclgrphProfInit](aclgrphProfInit.md)配对使用，先调用aclgrphProfInit接口再调用aclgrphProfFinalize接口，且只需要被调用一次。
