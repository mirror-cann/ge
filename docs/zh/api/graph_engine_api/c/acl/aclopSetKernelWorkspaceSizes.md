# aclopSetKernelWorkspaceSizes

## 产品支持情况

<!-- npu="950" id944 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id944 -->
<!-- npu="A3" id945 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id945 -->
<!-- npu="910b" id946 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id946 -->
<!-- npu="310b" id947 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id947 -->
<!-- npu="310p" id948 -->
- Atlas 推理系列产品：支持
<!-- end id948 -->
<!-- npu="910" id949 -->
- Atlas 训练系列产品：支持
<!-- end id949 -->
<!-- npu="IPV350" id950 -->
- IPV350：不支持
<!-- end id950 -->

## 功能说明

动态Shape场景下，设置算子Workspace参数。为非必须接口，根据算子情况可选。

## 函数原型

```c
aclError aclopSetKernelWorkspaceSizes(aclopKernelDesc *kernelDesc, int numWorkspaces, size_t *workspaceSizes)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| kernelDesc | 输入 | Kernel描述缓存，aclopKernelDesc类型的指针。<br>typedef struct aclopKernelDesc aclopKernelDesc; |
| numWorkspaces | 输入 | Workspaces个数。 |
| workspaceSizes | 输入 | Workspaces大小的数组地址指针。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
