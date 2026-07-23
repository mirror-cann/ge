# --fusion\_switch\_file

## 产品支持情况

全量芯片支持

## 功能说明

融合规则（包括图融合和UB融合）开关配置文件路径以及文件名，通过该参数**关闭**配置文件中指定的融合规则。

- 图融合：是FE根据融合规则进行改图的过程。图融合用融合后算子替换图中融合前算子，提升计算效率。图融合的场景如下：
  - 在某一些算子的数学计算量可以进行优化的情况下，可以进行图融合，融合后可以节省计算时间。例如：conv+biasAdd，可以融合成一个算子，直接在L0C中完成累加，从而省去add的计算过程。
  - 在融合后的计算过程可以通过硬件指令加速的情况下，可以进行图融合，融合后能够加速。例如：conv+biasAdd的累加过程，就是通过L0C中的累加功能进行加速的，可以通过图融合完成。

- UB融合：UB即AI处理器上的Unified Buffer，UB融合指A算子的计算结果在Unified Buffer上，需要搬移到Global Memory。B算子再执行时，需要将A算子的输出由Global Memory再搬移到Unified Buffer，进行B的计算逻辑，计算完之后，又从Unified Buffer搬移回Global Memory。

    从这个过程会发现A的结果从Unified Buffer-\>Global Memory-\>Unified Buffer-\>Global Memory。这个经过Global Memory进行数据搬移的过程是浪费的，因此将A和B算子合并成一个算子，省去了数据搬移的过程叫UB融合。UB融合可以减少整网中数据搬移的时间（Global Memory\>Unified Buffer，Unified Buffer-\>Global Memory），提高运算效率，有效降低带宽。

    <!-- npu="950" id1 -->
    Ascend 950PR/Ascend 950DT不支持UB融合。
    <!-- end id1 -->

## 关联参数

无。

## 参数取值

**参数值：**
配置文件路径以及文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束：**

系统内置的图融合和UB融合规则，均为默认开启，用户可以根据需要通过该参数关闭指定的融合规则。当前可以关闭的融合规则请参见《图融合和UB融合规则参考》，由于系统机制，其他融合规则无法关闭。

## 推荐配置及收益

无。

## 示例

- **场景1：逐条配置待关闭融合规则**

    配置文件样例如下，冒号前面为融合规则名，后面字段表示融合规则是否开启（融合规则开关配置文件名举例为fusion\_switch.cfg）：

    ```text
    xxxFusionPass:off
    yyyFusionPass:off
    ....
    ```

- **场景2：一键式关闭融合规则**

    该参数支持用户一键式关闭融合规则。

    ```json
    {
        "Switch":{
            "GraphFusion":{
                "ALL":"off"
            },
            "UBFusion":{
                "ALL":"off"
                }
        }
    }
    ```
  <!-- npu="950" id2 -->
  **不支持UB融合时，上述配置文件可以删除UBFusion。**
  <!-- end id2 -->

    说明：

    1. 关闭某些融合规则可能会导致功能问题，因此此处的一键式关闭仅关闭系统部分融合规则，而不是全部融合规则。
    2. 一键式关闭融合规则时，可以同时开启部分融合规则：

        配置文件样例：

        ```json
        {
            "Switch":{
                "GraphFusion":{
                    "ALL":"off",
                    "SoftmaxFusionPass":"on"
                },
                "UBFusion":{
                    "ALL":"off",
                    "TbePool2dQuantFusionPass":"on"
                }
            }
        }
        ```

        <!-- npu="950" id3 -->
        **不支持UB融合时，上述配置文件可以删除UBFusion。**
        <!-- end id3 -->

将上述配置好的_fusion\_switch.cfg_文件上传到ATC工具所在服务器任意目录，例如上传到$HOME/module，使用示例如下：

```bash
atc --fusion_switch_file=$HOME/module/fusion_switch.cfg ...
```

模型转换完毕，根据[--export\_compile\_stat](--export_compile_stat.md)参数的取值，决定是否生成算子融合信息结果文件"fusion\_result.json"。

该文件用于记录图编译过程中除去fusion\_switch.cfg文件中关闭的融合规则外，仍旧使用的融合规则，其中，"match\_times"字段表示模型转换过程中匹配到的融合规则次数，"effect\_times"字段表示实际生效的次数。如果未配置[--fusion\_switch\_file](--fusion_switch_file.md)参数，则生成的"fusion\_result.json"文件中记录模型转换过程中匹配到的所有融合规则。

## 使用约束

- 若网络模型中Convolution算子的“group”属性取值==模型文件prototxt中“num\_output”属性的取值，则上述配置文件中**VxxxRequantFusionPass**必须打开。

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--fusion_switch_file_res.md#id1 -->

<!-- npu="A3,910b,910,310p,310b" id8 -->
- AMCT对原始框架模型进行量化时，会插入量化和反量化算子，而使用ATC工具进行模型转换过程中，会对插入的量化和反量化算子进行融合，此情况下再进行量化后模型dump结果与原始模型dump结果的比对可能不准确，因此如果用户想使用AMCT量化后的模型进行精度比对，则需要通过[--fusion\_switch\_file](--fusion_switch_file.md)参数关闭部分融合功能，该场景下需要关闭的融合规则如下：

    <!-- npu="910" id4 -->
    Atlas 训练系列产品场景必须关闭的融合规则：

    ```text
    V100RequantFusionPass:off
    ConvConcatFusionPass:off
    SplitConvConcatFusionPass:off
    TbeEltwiseQuantFusionPass:off
    TbeConvDequantVaddReluQuantFusionPass:off
    TbeConvDequantVaddReluFusionPass:off
    TbeConvDequantQuantFusionPass:off
    TbeDepthwiseConvDequantFusionPass:off
    TbeFullyconnectionElemwiseDequantFusionPass:off
    TbeConv2DAddMulQuantPass:off
    TbePool2dQuantFusionPass:off
    TbeCommonRules0FusionPass:off
    TbeCommonRules2FusionPass:off
    ```
    <!-- end id4 -->

    <!-- npu="310p" id5 -->
    Atlas 推理系列产品必须关闭的融合规则：

    ```text
    V200RequantFusionPass:off
    ConvConcatFusionPass:off
    SplitConvConcatFusionPass:off
    TbeEltwiseQuantFusionPass:off
    TbeConvDequantVaddReluQuantFusionPass:off
    TbeConvDequantVaddReluFusionPass:off
    TbeConvDequantQuantFusionPass:off
    TbeDepthwiseConvDequantFusionPass:off
    TbeFullyconnectionElemwiseDequantFusionPass:off
    TbeConv2DAddMulQuantPass:off
    TbePool2dQuantFusionPass:off
    TbeCommonRules0FusionPass:off
    TbeCommonRules2FusionPass:off
    ```
    <!-- end id5 -->

    <!-- npu="910b,310b" id6 -->
    Atlas 200I/500 A2 推理产品、Atlas A2 训练系列产品/Atlas A2 推理系列产品必须关闭的融合规则：

    ```text
    ConvConcatFusionPass:off
    SplitConvConcatFusionPass:off
    TbeEltwiseQuantFusionPass:off
    TbeConvDequantVaddReluQuantFusionPass:off
    TbeConvDequantVaddReluFusionPass:off
    TbeConvDequantQuantFusionPass:off
    TbeDepthwiseConvDequantFusionPass:off
    TbeFullyconnectionElemwiseDequantFusionPass:off
    TbeConv2DAddMulQuantPass:off
    TbePool2dQuantFusionPass:off
    TbeCommonRules0FusionPass:off
    TbeCommonRules2FusionPass:off
    ```
    <!-- end id6 -->

    <!-- npu="A3" id7 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品必须关闭的融合规则：

    ```text
    ConvConcatFusionPass:off
    SplitConvConcatFusionPass:off
    TbeEltwiseQuantFusionPass:off
    TbeConvDequantVaddReluQuantFusionPass:off
    TbeConvDequantVaddReluFusionPass:off
    TbeConvDequantQuantFusionPass:off
    TbeDepthwiseConvDequantFusionPass:off
    TbeFullyconnectionElemwiseDequantFusionPass:off
    TbeConv2DAddMulQuantPass:off
    TbePool2dQuantFusionPass:off
    TbeCommonRules0FusionPass:off
    TbeCommonRules2FusionPass:off
    ```
    <!-- end id7 -->

    融合规则简述如下，详细描述请参见《图融合和UB融合规则参考》。

  <!-- npu="910,310p" id10 -->
  - V100RequantFusionPass

    图融合规则，在AscendDequant的输入插入RequantHostCpuOpV2算子。

    该融合规则仅在Atlas 训练系列产品、Atlas 推理系列产品支持。
  <!-- end id10 -->

  <!-- npu="310p" id9 -->
  - V200RequantFusionPass

    图融合规则，将AscendDequant和AscendQuant融合成AscendRequant，在AscendDequant的输入插入RequantHostCpuOpV2Re算子。

    该融合规则仅在Atlas 推理系列产品支持。
  <!-- end id9 -->

  - ConvConcatFusionPass

    图融合规则，支持conv2d\*N+concat算子的图融合规则，conv2d后面可以连接dequant和Relu类算子。

  - SplitConvConcatFusionPass

    图融合规则，支持split+conv2d\*N+concat算子的融合规则，conv2d后面可以连接dequant和Relu类算子。

  - TbeEltwiseQuantFusionPass

    UB融合规则，支持elemwise+quant算子的UB融合，quant算子为可选节点。

  - TbeConvDequantVaddReluQuantFusionPass

    UB融合规则，量化场景下，对Conv-dequant-vadd-relu-quant连续的节点，标记UB融合，提升推理性能。

  - TbeConvDequantVaddReluFusionPass

    UB融合规则，支持conv2d+dequant+vadd+relu/conv2d+dequant+\(leakyrelu\)+vadd算子的融合节点。

  - TbeConvDequantQuantFusionPass

    UB融合规则，量化场景下，对Conv-dequant-quant连续的节点，标记UB融合，提升推理性能。

  - TbeDepthwiseConvDequantFusionPass

    UB融合规则，支持depthwiseConv2d+dequant+\(relu/mul\)+quant/depthwiseConv2d+dequant+\(sigmoid\)+mul/depthwiseConv2d+requant/depthwiseConv2d+\(power+relu6+power\)+elemwise+\(quant\)算子的融合节点。

  - TbeFullyconnectionElemwiseDequantFusionPass

    UB融合规则，支持如下两种形式的融合：

    1. 静态shape场景BatchMatMul/BatchMatMulV2 + elemwise的融合。
    2. 静态shape场景MatMul/MatMulV2/BatchMatMul/BatchMatMulV2 + AscendDequant + elemwise1\(+ elemwise2\)的融合。

  - TbeConv2DAddMulQuantPass

    UB融合规则，支持conv+dequant+add+quant融合，add算子除quant外还必须有另两路任意输出才可以进行融合。

  - TbePool2dQuantFusionPass

    UB融合规则，量化场景下，对Pool2d-quant连续的节点，标记UB融合，提升推理性能。

  - TbeCommonRules0FusionPass

    UB融合规则，支持StridedRead+Conv2D+dequant+elemwise+quant+StridedWrite算子的UB融合，除Conv2D外，其他节点都是可选节点。

  - TbeCommonRules2FusionPass

    UB融合规则，支持StridedRead+Conv2D+dequant+elemwise+quant+StridedWrite算子的UB融合，除Conv2D外，其他节点都是可选节点；elemwise支持多输出场景下的融合。

  <!-- end id8 -->
