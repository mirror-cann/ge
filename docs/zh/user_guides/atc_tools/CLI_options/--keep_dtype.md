# --keep\_dtype

## 产品支持情况

全量芯片支持

## 功能说明

通过配置文件指定原始模型中特定算子的数据类型在模型编译过程中保持不变。

推理场景下，使用[--precision\_mode](--precision_mode.md)或[--precision\_mode\_v2](--precision_mode_v2.md)参数设置整个网络模型的精度模式，可能会有个别算子存在性能或精度问题，为此引入[--keep\_dtype](--keep_dtype.md)参数，保持原始网络模型编译时个别算子的计算精度不变，若原始网络模型中算子的计算精度，在AI处理器上不支持，则系统内部会自动采用算子支持的高精度来计算。

## 关联参数

- 该参数需要与[--precision\_mode](--precision_mode.md)或[--precision\_mode\_v2](--precision_mode_v2.md)参数配合使用，但当[--precision\_mode](--precision_mode.md)取值为must\_keep\_origin\_dtype或[--precision\_mode\_v2](--precision_mode_v2.md)取值为origin时，--keep\_dtype参数不生效。
- [--customize\_dtypes](--customize_dtypes.md)参数与[--keep\_dtype](--keep_dtype.md)参数都用于设置算子的计算精度，若涉及需提升模型推理精度的场景，建议先使用[--keep\_dtype](--keep_dtype.md)参数保持原图精度，若精度依然得不到提升，可以尝试使用[--customize\_dtypes](--customize_dtypes.md)参数自定义某个或某些算子的计算精度。

    但需注意，使用[--customize\_dtypes](--customize_dtypes.md)参数且通过配置算子名称的方式，可能会由于内部模型优化过程中的融合、拆分等操作导致算子名称发生变化，进而导致配置不生效，未达到提升精度的目的，可进一步获取日志定位问题，关于日志的详细说明请参见[《日志参考》](https://hiascend.com/document/redirect/CannCommunitylogref)。

- 若同时使用了[--customize\_dtypes](--customize_dtypes.md)参数与[--keep\_dtype](--keep_dtype.md)参数，则以[--customize\_dtypes](--customize_dtypes.md)参数设置的精度为准。

## 参数取值

**参数值：**
算子配置文件路径以及文件名，配置文件中列举需保持计算精度的算子名称或算子类型，每个算子单独一行。

**参数值约束：**
若为算子类型，则以**OpType::TypeName**格式进行配置，每个OpType单独一行，且算子OpType必须为基于Ascend IR定义的算子的OpType，算子类型查看方法请参见[如何确定原始框架网络模型中的算子与AI处理器支持的算子的对应关系](../FAQ/operator_correspondence_guide.md)。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

## 推荐配置及收益

无。

## 示例

- 若配置文件中为算子名称，则配置样例如下（文件名举例为_exceptionlist.cfg_）：

    ```text
    Opname1
    Opname2
    …
    ```

- 若配置文件中为算子类型，则配置样例为（文件名举例为_exceptionlist.cfg_）：

    ```text
    OpType::TypeName1
    OpType::TypeName2
    …
    ```

以TensorFlow ResNet50网络模型中的Relu算子为例，其对应的Ascend IR定义的算子类型为Relu，配置样例如下：

```text
#算子名称配置样例：
fp32_vars/Relu
#算子类型配置样例：
OpType::Relu
```

将配置好的_exceptionlist.cfg_文件上传到ATC工具所在服务器任意目录，例如上传到$HOME，使用示例如下：

```bash
atc --keep_dtype=$HOME/exceptionlist.cfg --precision_mode=force_fp16 ...
```

模型编译时，_exceptionlist.cfg_文件中的算子，保持原始网络模型精度，即精度不会改变，其余网络模型中的算子以[--precision\_mode](--precision_mode.md)或[--precision\_mode\_v2](--precision_mode_v2.md)参数指定的精度模式进行编译。

## 依赖约束

无。
