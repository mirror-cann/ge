# aclmdlGetOutputNameByIndex

## 产品支持情况

<!-- npu="950" id748 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id748 -->
<!-- npu="A3" id749 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id749 -->
<!-- npu="910b" id750 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id750 -->
<!-- npu="310b" id751 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id751 -->
<!-- npu="310p" id752 -->
- Atlas 推理系列产品：支持
<!-- end id752 -->
<!-- npu="910" id753 -->
- Atlas 训练系列产品：支持
<!-- end id753 -->
<!-- npu="IPV350" id754 -->
- IPV350：支持
<!-- end id754 -->

## 功能说明

根据模型描述信息获取模型中指定输出的输出算子名称、算子输出边的下标、top名称或输出名称。

## 函数原型

```c
const char *aclmdlGetOutputNameByIndex(const aclmdlDesc *modelDesc, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| modelDesc | 输入 | aclmdlDesc类型的指针。<br>需提前调用[aclmdlCreateDesc](aclmdlCreateDesc.md)接口创建aclmdlDesc类型的数据。 |
| index | 输入 | 指定获取第几个输出的输出算子名称、算子输出边下标、top名称或输出名称，index值从0开始。 |

## 返回值说明

返回指定输出的输出算子名称、算子输出边的下标、top名称或输出名称。不同原始网络、不同构建模型的方式，调用本接口获取的返回值格式不同。

- Caffe网络

    返回值格式如下，各项之间以冒号分割，如果模型中包含top名称就返回，不包含就不返回：

    输出算子名称 : 算子输出边下标 : top名称

- TensorFlow网络
    - 使用ATC工具构建om模型的场景下，返回值格式如下，各项之间以冒号分割：

        输出算子名称 : 算子输出边下标

    - 使用构图接口构建om模型的场景下，返回值格式如下，各项之间以下划线分割：

        output_网络输出下标_输出算子名称_算子输出边下标

        构图接口的详细说明请参见[《图开发》](https://hiascend.com/document/redirect/CannCommunityGraphguide)。

- ONNX网络
    - 在构建模型时，不指定输出节点名称（node\_name）或输出名称（output的name），或者仅指定输出名称，返回值格式如下，各项之间以冒号分割：

        输出算子名称 : 算子输出边下标 : 输出名称

    - 在构建模型时，指定输出节点名称（node\_name）：

        输出算子名称可能是图融合后的算子名称，也可能是子图名称。

        - 使用ATC工具构建om模型的场景下，返回值格式如下，各项之间以冒号分割：

            输出算子名称 : 算子输出边下标

        - 使用构图接口构建om模型的场景下，返回值格式如下，各项之间以下划线分割：

            output_网络输出下标_输出算子名称_算子输出边下标

            构图接口的详细说明请参见[《图开发》](https://hiascend.com/document/redirect/CannCommunityGraphguide)。

    - 同时指定输出节点名称（node\_name）和输出名称（output的name），接口返回报错。
