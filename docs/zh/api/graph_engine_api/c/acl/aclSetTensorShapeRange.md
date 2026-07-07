# aclSetTensorShapeRange

## 产品支持情况

<!-- npu="950" id386 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id386 -->
<!-- npu="A3" id387 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id387 -->
<!-- npu="910b" id388 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id388 -->
<!-- npu="310b" id389 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id389 -->
<!-- npu="310p" id390 -->
- Atlas 推理系列产品：支持
<!-- end id390 -->
<!-- npu="910" id391 -->
- Atlas 训练系列产品：支持
<!-- end id391 -->
<!-- npu="IPV350" id392 -->
- IPV350：x
<!-- end id392 -->

## 功能说明

调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建tensor描述信息后，可通过本接口设置tensor的各个维度的取值范围。

使用场景：动态Shape的算子，其输入Shape中变化维度用-1表示，但每个变化维度的范围是不一样的，需要显式设置，如Shape为[16,-1,20,-1]，对应的shape range可以是[[16,16],[1,128],[20,20],[1,10]\]，表示第一维的Shape范围固定为16，第二维的Shape范围为1到128，第三维的Shape范围固定为20，第四维的Shape范围为1到10。

## 函数原型

```c
aclError aclSetTensorShapeRange(aclTensorDesc* desc, size_t dimsCount, int64_t dimsRange[][ACL_TENSOR_SHAPE_RANGE_NUM])
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| dimsCount | 输入 | tensor中dims的维度个数。 |
| dimsRange | 输入 | dimsRange为每个维度的取值范围，用二维数组表示范围。<br>如果Shape中的维度值不是-1时，表示固定维度，该维度对应的shape range中最大值和最小值相同；否则，表示动态维度，数组的两个维度分别表示对应tensor dims中的最小值和最大值。<br>#define ACL_TENSOR_SHAPE_RANGE_NUM 2 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
