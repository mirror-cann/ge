# aclgrphProfInit

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
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphProfInit_res.md#id1 -->

## 头文件/库文件

- 头文件：\#include <ge/ge\_prof.h\>
- 库文件：libmsprofiler.so

## 功能说明

初始化Profiling，设置Profiling参数（目前供用户设置保存性能数据文件的路径）。

## 函数原型

```c++
Status aclgrphProfInit(const char *profiler_path, uint32_t length)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| profiler_path | 输入 | 指定保存性能数据的文件的路径，路径支持绝对路径和相对路径。 |
| length | 输入 | profiler_path的长度，单位为字节。最大长度不超过4096字节。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功。<br>FAILED：失败。<br>ACL_ERROR_FEATURE_UNSUPPORTED：动态Profiling场景下不支持调用aclgrphProfInit接口。 |

## 约束说明

- 不支持多次重复调用aclgrphProfInit，并且该接口需和[aclgrphProfFinalize](aclgrphProfFinalize.md)配对使用，先调用aclgrphProfInit接口再调用aclgrphProfFinalize接口。
- 建议该接口在[GEInitialize](./Session/GEInitialize.md)之后，[AddGraph](./Session/AddGraph.md)之前被调用，可采集到AddGraph时的Profiling数据。
