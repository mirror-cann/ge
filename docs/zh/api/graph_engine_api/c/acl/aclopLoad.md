# aclopLoad

## 产品支持情况

<!-- npu="950" id797 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id797 -->
<!-- npu="A3" id798 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id798 -->
<!-- npu="910b" id799 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id799 -->
<!-- npu="310b" id800 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id800 -->
<!-- npu="310p" id801 -->
- Atlas 推理系列产品：支持
<!-- end id801 -->
<!-- npu="910" id802 -->
- Atlas 训练系列产品：支持
<!-- end id802 -->
<!-- npu="IPV350" id803 -->
- IPV350：不支持
<!-- end id803 -->

## 功能说明

从内存中加载单算子模型数据（单算子模型数据是指“单算子编译成\*.om文件后，再将om文件读取到内存中”的数据），由用户管理内存。

## 函数原型

```c
aclError aclopLoad(const void *model,  size_t modelSize)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| model | 输入 | 单算子模型数据的内存地址指针。 |
| modelSize | 输入 | 内存中的模型数据长度，单位Byte。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

模型加载环境中的算子库版本需与模型编译环境的算子库版本一致，否则在加载算子时会报错。可通过\$\{INSTALL\_DIR\}/opp/version.info文件中的version字段查看算子库版本。\$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
在加载前，请先根据单算子om文件的大小评估内存空间是否足够，内存空间不足，会导致应用程序异常。针对不同产品型号，一个进程内正在执行的算子的最大个数上限不同：
<!-- end id1 -->

<!-- npu="950,A3,910b" id2 -->
- 对于Ascend 950PR/Ascend 950DT、Atlas A3 训练系列产品/Atlas A3 推理系列产品、Atlas A2 训练系列产品/Atlas A2 推理系列产品，最大个数为2000000。
<!-- end id2 -->
<!-- npu="910,310b" id3 -->
- 对于Atlas 200I/500 A2 推理产品、Atlas 训练系列产品，最大个数为40000000。
<!-- end id3 -->
<!-- npu="310p" id4 -->
- 对于Atlas 推理系列产品，Ascend EP形态下，上限是40000000；Control CPU开放形态下，上限是2000000。
<!-- end id4 -->
