# aclGetCustomOpLibPath

## 产品支持情况

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 200I/500 A2 推理产品：不支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持
- MC62CM12A AI处理器：不支持
- BS9SX2A AI处理器：不支持
- BS9SX1A AI处理器：不支持
- IPV350：不支持

## 头文件

\#include <register/register\_base.h\>

## 功能说明

获取自定义算子库的路径。

该接口为内部框架会调用的接口，称之为**内部关联接口**。开发者不会直接调用内部关联接口，无需关注。

## 函数原型

```c
const char *aclGetCustomOpLibPath()
```

## 参数说明

无

## 返回值说明

返回自定义算子库的路径。多个路径之间用英文冒号分隔，路径已按照用户设置的算子优先级顺序排列好，优先级越高的位置越靠前。

关于自定义算子的优先级设置请参考《Ascend C算子开发指南》中的“编程指南 \> 附录 \> 工程化算子开发 \>  算子动态库编译”章节。

## 约束说明

无

## 调用示例

```c
const char* path = aclGetCustomOpLibPath();
```
