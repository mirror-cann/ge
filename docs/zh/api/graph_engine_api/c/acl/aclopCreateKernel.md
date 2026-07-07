# aclopCreateKernel

## 产品支持情况

<!-- npu="950" id64 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id64 -->
<!-- npu="A3" id65 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id65 -->
<!-- npu="910b" id66 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id66 -->
<!-- npu="310b" id67 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id67 -->
<!-- npu="310p" id68 -->
- Atlas 推理系列产品：支持
<!-- end id68 -->
<!-- npu="910" id69 -->
- Atlas 训练系列产品：支持
<!-- end id69 -->
<!-- npu="IPV350" id70 -->
- IPV350：不支持
<!-- end id70 -->

## 功能说明

动态Shape场景下，将算子注册到系统内部，运行算子时使用。

## 函数原型

```c
aclError aclopCreateKernel(const char *opType,
const char *kernelId,
const char *kernelName,
void *binData,
int binSize,
aclopEngineType enginetype,
aclDataDeallocator deallocator)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opType | 输入 | 算子类型的指针。 |
| kernelId | 输入 | 算子执行时要指定的Kernel ID的指针。 |
| kernelName | 输入 | 算子Kernel名称的指针，和算子二进制文件中的kernelName保持一致。 |
| binData | 输入 | 算子Kernel文件的内存地址指针。 |
| binSize | 输入 | 算子Kernel文件的内存大小，单位为Byte。 |
| enginetype | 输入 | 表示算子执行引擎。类型定义请参见[aclopEngineType](aclopEngineType.md)。 |
| deallocator | 输入 | 释放binData内存的回调函数，调用者根据binData的构造方法设置对应的释放函数。如果数据由调用者释放则设置为空。 |

<br>

回调函数原型如下：

```c
typedef void (*aclDataDeallocator)(void *data, size_t length);
```

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
