# aclmdlGetDynamicHW

## 产品支持情况

<!-- npu="A3" id512 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id512 -->
<!-- npu="910b" id513 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id513 -->
<!-- npu="310b" id514 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id514 -->
<!-- npu="310p" id515 -->
- Atlas 推理系列产品：支持
<!-- end id515 -->
<!-- npu="910" id516 -->
- Atlas 训练系列产品：支持
<!-- end id516 -->
<!-- npu="IPV350" id517 -->
- IPV350：不支持
<!-- end id517 -->

## 功能说明

根据模型描述信息获取模型支持的动态宽高信息。

## 函数原型

```c
aclError aclmdlGetDynamicHW(const aclmdlDesc *modelDesc, size_t index, aclmdlHW *hw)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 预留参数，当前未使用，固定设置为-1。 |
| hw | 输出 | hwCount等于0时，表示不支持设置档位信息，以模型中的档位为准。 |

<br>

aclmdlHW类型的定义如下：

```c
const int ACL_MAX_HW_NUM = 128;
typedef struct aclmdlHW {
    uint64_t hw[ACL_MAX_HW_NUM][2]; /** 模型中支持的具体分档，每组分档中，数组下标为0代表的是高，下标为1代表的是宽 */
} aclmdlHW;
```

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
