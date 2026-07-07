# --customize\_dtypes

## 产品支持情况

全量芯片支持

## 功能说明

模型编译时自定义某个或某些算子的计算精度。

## 关联参数

- 若本参数与[--precision\_mode](--precision_mode.md)或[--precision\_mode\_v2](--precision_mode_v2.md)配合使用时，除本参数指定的算子外，模型中其它算子按[--precision\_mode](--precision_mode.md)或[--precision\_mode\_v2](--precision_mode_v2.md)参数配置的精度模式来编译。
- [--customize\_dtypes](--customize_dtypes.md)参数与[--keep\_dtype](--keep_dtype.md)参数都用于设置算子的计算精度，若涉及需提升模型推理精度的场景，建议先使用[--keep\_dtype](--keep_dtype.md)参数保持原图精度，若精度依然得不到提升，可以尝试使用[--customize\_dtypes](--customize_dtypes.md)参数自定义某个或某些算子的计算精度。

    但需注意，使用[--customize\_dtypes](--customize_dtypes.md)参数且通过配置算子名称的方式，可能会由于内部模型优化过程中的融合、拆分等操作导致算子名称发生变化，进而导致配置不生效，未达到提升精度的目的，可进一步获取日志定位问题，关于日志的详细说明请参见《日志参考》。

- 若同时使用了[--customize\_dtypes](--customize_dtypes.md)参数与[--keep\_dtype](--keep_dtype.md)参数，则以[--customize\_dtypes](--customize_dtypes.md)参数设置的精度为准。

## 参数取值

**参数值：**
算子配置文件路径以及文件名，配置文件中列举需要自定义计算精度的算子名称或算子类型，每个算子单独一行。

**参数值约束：**

- 若为算子名称，以**Opname::InputDtype:dtype1,...,OutputDtype:dtype1,...**格式进行配置，每个Opname单独一行，dtype1，dtype2...需要与可设置计算精度的算子输入，算子输出的个数一一对应**。**
- 若为算子类型，以**OpType::TypeName:InputDtype:dtype1,...,OutputDtype:dtype1,...**格式进行配置，每个OpType单独一行，dtype1，dtype2...需要与可设置计算精度的算子输入，算子输出的个数一一对应，且算子OpType必须为基于Ascend IR定义的算子的OpType，算子类型查看方法请参见[如何确定原始框架网络模型中的算子与AI处理器支持的算子的对应关系](../FAQ/operator_correspondence_guide.md)。
- 对于同一个算子，如果同时配置了**Opname**和**OpType**的配置项，编译时以**Opname**的配置项为准。
- 使用该参数指定某个算子的计算精度时，如果模型转换过程中该算子被融合掉，则该算子指定的计算精度不生效。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、英文冒号\(:\)、中文汉字。

## 推荐配置及收益

无。

## 示例

- 若配置文件中为算子名称，则配置样例为（文件名举例为_customize\_dtypes.cfg_）：

    ```text
    Opname1::InputDtype:dtype1,dtype2,...,OutputDtype:dtype1,...
    Opname2::InputDtype:dtype1,dtype2,...,OutputDtype:dtype1,...
    ```

- 若配置文件中为算子类型，则配置样例为（文件名举例为_customize\_dtypes.cfg_）：

    ```text
    OpType::TypeName1:InputDtype:dtype1,dtype2,...,OutputDtype:dtype1,...
    OpType::TypeName2:InputDtype:dtype1,dtype2,...,OutputDtype:dtype1,...
    ```

算子具体支持的计算精度可以从[《算子库》](https://hiascend.com/document/redirect/CannCommunityOplist)\> “Ascend IR算子规格说明”中查看。

以TensorFlow ResNet50网络模型中的Relu算子为例，其对应的Ascend IR定义的算子类型为Relu，该算子输入和输出只有一个，该配置样例如下：

- 算子名称配置样例：

    ```text
    fp32_vars/Relu::InputDtype:float16,OutputDtype:int8
    ```

- 算子类型配置样例：

    ```text
    OpType::Relu:InputDtype:float16,OutputDtype:int8
    ```

将配置好的_customize\_dtypes.cfg_文件上传到ATC工具所在服务器的任意目录，例如上传到$HOME，使用示例如下：

```basj
atc --customize_dtypes=$HOME/customize_dtypes.cfg --precision_mode=force_fp16 ...
```

模型编译时，_customize\_dtypes.cfg_文件中的算子，使用指定的计算精度，其余网络模型中的算子以[--precision\_mode](--precision_mode.md)或[--precision\_mode\_v2](--precision_mode_v2.md)参数指定的精度模式进行编译。

## 使用约束

1. 使用该参数指定算子的计算精度，由于其优先级高于[--precision\_mode](--precision_mode.md)或[--precision\_mode\_v2](--precision_mode_v2.md)、[--keep\_dtype](--keep_dtype.md)参数，可能会导致后续推理精度或者性能的下降。
2. 使用该参数指定算子的计算精度，如果指定的精度算子本身不支持，则会导致模型编译失败。
