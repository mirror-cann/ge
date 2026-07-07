# aclmdlSetAIPPCscParams

## 产品支持情况

<!-- npu="950" id902 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id902 -->
<!-- npu="A3" id903 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id903 -->
<!-- npu="910b" id904 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id904 -->
<!-- npu="310b" id905 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id905 -->
<!-- npu="310p" id906 -->
- Atlas 推理系列产品：支持
<!-- end id906 -->
<!-- npu="910" id907 -->
- Atlas 训练系列产品：支持
<!-- end id907 -->
<!-- npu="IPV350" id908 -->
- IPV350：不支持
<!-- end id908 -->

## 功能说明

动态AIPP场景下，设置CSC色域转换相关的参数，若色域转换开关关闭，则调用该接口设置以下参数无效。

```bash
YUV转BGR：
| B |   | cscMatrixR0C0 cscMatrixR0C1 cscMatrixR0C2 | | Y - cscInputBiasR0 |
| G | = | cscMatrixR1C0 cscMatrixR1C1 cscMatrixR1C2 | | U - cscInputBiasR1 | >> 8
| R |   | cscMatrixR2C0 cscMatrixR2C1 cscMatrixR2C2 | | V - cscInputBiasR2 |
BGR转YUV：
| Y |   | cscMatrixR0C0 cscMatrixR0C1 cscMatrixR0C2 | | B |        | cscOutputBiasR0 |
| U | = | cscMatrixR1C0 cscMatrixR1C1 cscMatrixR1C2 | | G | >> 8 + | cscOutputBiasR1 |
| V |   | cscMatrixR2C0 cscMatrixR2C1 cscMatrixR2C2 | | R |        | cscOutputBiasR2 |
```

色域转换参数值与转换前图片的格式、转换后图片的格式强相关，您可以参考[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“高级功能 \> 开启AIPP \> 色域转换配置说明”，根据转换前图片格式、转换后图片格式来配置色域转换参数。如果手册中列举的图片格式不满足要求，您需自行根据实际需求配置色域转换参数。

## 函数原型

```c
aclError aclmdlSetAIPPCscParams(aclmdlAIPP *aippParmsSet, int8_t cscSwitch,
int16_t cscMatrixR0C0, int16_t cscMatrixR0C1, int16_t cscMatrixR0C2,
int16_t cscMatrixR1C0, int16_t cscMatrixR1C1,int16_t cscMatrixR1C2,
int16_t cscMatrixR2C0, int16_t cscMatrixR2C1, int16_t cscMatrixR2C2,
uint8_t cscOutputBiasR0, uint8_t cscOutputBiasR1, uint8_t cscOutputBiasR2,
uint8_t cscInputBiasR0, uint8_t cscInputBiasR1, uint8_t cscInputBiasR2)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| aippParmsSet | 输出 | 动态AIPP参数对象的指针。<br>需提前调用[aclmdlCreateAIPP](aclmdlCreateAIPP.md)接口创建aclmdlAIPP类型的数据。 |
| cscSwitch | 输入 | 色域转换开关，取值范围：<br><br>  - 0：关闭色域转换开关，设置为0时，则设置以下参数无效<br>  - 1：打开色域转换开关 |
| cscMatrixR0C0 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR0C1 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR0C2 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR1C0 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR1C1 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR1C2 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR2C0 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR2C1 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscMatrixR2C2 | 输入 | 色域转换矩阵参数。<br>取值范围：[-32677 ,32676] |
| cscOutputBiasR0 | 输入 | RGB转YUV时的输出偏移。<br>取值范围：[0, 255] |
| cscOutputBiasR1 | 输入 | RGB转YUV时的输出偏移。<br>取值范围：[0, 255] |
| cscOutputBiasR2 | 输入 | RGB转YUV时的输出偏移。<br>取值范围：[0, 255] |
| cscInputBiasR0 | 输入 | YUV转RGB时的输入偏移。<br>取值范围：[0, 255] |
| cscInputBiasR1 | 输入 | YUV转RGB时的输入偏移。<br>取值范围：[0, 255] |
| cscInputBiasR2 | 输入 | YUV转RGB时的输入偏移。<br>取值范围：[0, 255] |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。

## 约束说明

如果通过[aclmdlSetAIPPInputFormat](aclmdlSetAIPPInputFormat.md)接口设置的原始图像格式为YUV400，则不支持通过本接口设置色域转换参数。
