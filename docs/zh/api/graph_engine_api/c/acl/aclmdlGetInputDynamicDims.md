# aclmdlGetInputDynamicDims

## 产品支持情况

<!-- npu="950" id734 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id734 -->
<!-- npu="A3" id735 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id735 -->
<!-- npu="910b" id736 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id736 -->
<!-- npu="310b" id737 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id737 -->
<!-- npu="310p" id738 -->
- Atlas 推理系列产品：支持
<!-- end id738 -->
<!-- npu="910" id739 -->
- Atlas 训练系列产品：支持
<!-- end id739 -->
<!-- npu="IPV350" id740 -->
- IPV350：支持
<!-- end id740 -->

## 功能说明

根据模型描述信息获取模型的输入所支持的动态维度信息。

## 函数原型

```c
aclError aclmdlGetInputDynamicDims(const aclmdlDesc *modelDesc, size_t index, aclmdlIODims *dims, size_t gearCount)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 预留参数，当前未使用，固定设置为-1。 |
| dims | 输出 | 输入的动态维度信息的指针。类型定义请参见[aclmdlIODims](aclmdlIODims.md)。<br>dims参数是一个数组，数组中的每个元素指向[aclmdlIODims](aclmdlIODims.md)结构，[aclmdlIODims](aclmdlIODims.md)结构体中的dims参数也是一个数组，该数组中的每个元素对应每一档中的具体值。<br>例如：<br>aclmdlIODims dims[gearCount];<br>aclmdlGetInputDynamicDims(model.modelDesc, -1, dims, gearCount); |
| gearCount | 输入 | 模型支持的动态维度档位数，需要先通过[aclmdlGetInputDynamicGearCount](aclmdlGetInputDynamicGearCount.md)接口获取。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

只有在模型编译时设置了动态维度的分档信息后，才可以调用该接口获取动态维度信息。

例如，模型有三个输入，分别为data\(1, 1, 40, -1\)，label\(1, -1\)，mask\(-1, -1\) ， 其中-1表示动态可变。在模型转换时，dynamic\_dims参数的配置示例为：--dynamic\_dims="20,20,1,1;40,40,2,2;80,60,4,4"，则通过本接口获取的动态维度信息为（**[aclmdlIODims](aclmdlIODims.md)**结构体内的name暂不使用）：

- 第0档：
    - **[aclmdlIODims](aclmdlIODims.md)**结构体内dimCount：8，表示所有输入tensor的维度数量之和
    - **[aclmdlIODims](aclmdlIODims.md)**结构体内的dims：“1,1,40,20,1,20,1,1”，表示data\(1,1,40,20\)+label\(1,20\)+mask\(1,1\)

- 第1档：
    - **[aclmdlIODims](aclmdlIODims.md)**结构体内dimCount：8，表示所有输入tensor的维度数量之和
    - **[aclmdlIODims](aclmdlIODims.md)**结构体内的dims：“1,1,40,40,1,40,2,2”，表示data\(1,1,40,40\)+label\(1,40\)+mask\(2,2\)

- 第2档：
    - **[aclmdlIODims](aclmdlIODims.md)**结构体内dimCount：8，表示所有输入tensor的维度数量之和
    - **[aclmdlIODims](aclmdlIODims.md)**结构体内的dims：“1,1,40,80,1,60,4,4”，表示data\(1,1,40,80\)+label\(1,60\)+mask\(4,4\)
