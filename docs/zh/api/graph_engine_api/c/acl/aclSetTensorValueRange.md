# aclSetTensorValueRange

## 产品支持情况

<!-- npu="950" id1035 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1035 -->
<!-- npu="A3" id1036 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id1036 -->
<!-- npu="910b" id1037 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id1037 -->
<!-- npu="310b" id1038 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id1038 -->
<!-- npu="310p" id1039 -->
- Atlas 推理系列产品：支持
<!-- end id1039 -->
<!-- npu="910" id1040 -->
- Atlas 训练系列产品：支持
<!-- end id1040 -->
<!-- npu="IPV350" id1041 -->
- IPV350：不支持
<!-- end id1041 -->

## 功能说明

调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建tensor描述信息后，可通过本接口设置tensor的数据值的范围。

使用场景：部分算子input的值就是该算子的输出Shape，在动态Shape场景下，这个Shape的值就有一个取值范围，在执行算子时，需要在input上设置Shape值的范围（例如：[[16,16],[1,128],[20,20],[1,10]\]），这样才能正常编译、执行算子。

## 函数原型

```c
aclError aclSetTensorValueRange(aclTensorDesc* desc, size_t valueCount, int64_t valueRange[][ACL_TENSOR_VALUE_RANGE_NUM])
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| desc | 输出 | aclTensorDesc类型的指针。<br>需提前调用[aclCreateTensorDesc](aclCreateTensorDesc.md)接口创建aclTensorDesc类型。 |
| valueCount | 输入 | 需设置范围的数据值的个数。 |
| valueRange | 输入 | valueRange为每个数据值的范围，用二维数组表示范围。<br>#define ACL_TENSOR_VALUE_RANGE_NUM 2 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
