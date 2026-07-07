# aclmdlGetDynamicBatch

## 产品支持情况

<!-- npu="A3" id588 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id588 -->
<!-- npu="910b" id589 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id589 -->
<!-- npu="310b" id590 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id590 -->
<!-- npu="310p" id591 -->
- Atlas 推理系列产品：支持
<!-- end id591 -->
<!-- npu="910" id592 -->
- Atlas 训练系列产品：支持
<!-- end id592 -->
<!-- npu="IPV350" id593 -->
- IPV350：不支持
<!-- end id593 -->

## 功能说明

根据模型描述信息获取模型支持的动态Batch信息。

## 函数原型

```c
aclError aclmdlGetDynamicBatch(const aclmdlDesc *modelDesc, aclmdlBatch *batch)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| batch | 输出 | batchCount等于0时，表示不支持设置档位信息，以模型中的档位为准。 |

<br>

aclmdlBatch类型的定义如下：

```c
const int ACL_MAX_BATCH_NUM = 128;
    size_t batchCount; /** 模型中支持的batch分档数 */
    uint64_t batch[ACL_MAX_BATCH_NUM]; /** 模型中支持的具体分档 */
} aclmdlBatch;
```

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
