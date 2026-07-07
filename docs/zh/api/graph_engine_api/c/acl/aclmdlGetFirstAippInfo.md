# aclmdlGetFirstAippInfo

## 产品支持情况

<!-- npu="950" id930 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id930 -->
<!-- npu="A3" id931 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id931 -->
<!-- npu="910b" id932 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id932 -->
<!-- npu="310b" id933 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id933 -->
<!-- npu="310p" id934 -->
- Atlas 推理系列产品：支持
<!-- end id934 -->
<!-- npu="910" id935 -->
- Atlas 训练系列产品：支持
<!-- end id935 -->
<!-- npu="IPV350" id936 -->
- IPV350：不支持
<!-- end id936 -->

## 功能说明

获取模型AIPP（包括静态AIPP和动态AIPP）的配置信息。获取模型中动态AIPP的信息时，只能获取[aclAippInfo](aclAippInfo.md)结构体中如下参数的值：srcFormat、srcDatatype、srcDimNum、shapeCount、outDims，其它参数值无效。

AIPP支持的几种操作的计算方式及其计算顺序如下：抠图-\>色域转换-\>减均值/归一化-\>padding。

## 函数原型

```c
aclError aclmdlGetFirstAippInfo(uint32_t modelId, size_t index, aclAippInfo *aippInfo)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelId | 输入 | 模型ID。<br>调用[aclmdlLoadFromFile](aclmdlLoadFromFile.md)接口/[aclmdlLoadFromMem](aclmdlLoadFromMem.md)接口/[aclmdlLoadFromFileWithMem](aclmdlLoadFromFileWithMem.md)接口/[aclmdlLoadFromMemWithMem](aclmdlLoadFromMemWithMem.md)接口加载模型成功后，会返回模型ID。 |
| index | 输入 | 模型中输入的index。 |
| aippInfo | 输出 | 获取指定输入上AIPP配置信息的指针。类型定义请参见[aclAippInfo](aclAippInfo.md)。<br>详细说明及参数解释，请参考[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“高级功能 > 开启AIPP”。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
