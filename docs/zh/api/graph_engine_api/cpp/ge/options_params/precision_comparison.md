# 精度比对

## ge.bufferOptimize

数据缓存优化开关。

**参数取值：**

- l1\_optimize：表示开启l1优化。当前版本该参数无效，等同于off\_optimize。
- l2\_optimize：（默认值）表示开启l2优化。
- off\_optimize：表示关闭数据缓存优化。

**使用建议：**

建议打开数据缓存优化功能：开启数据缓存优化可提高计算效率、提升性能，但由于部分算子在实现上可能存在未考虑的场景，导致影响精度，因此在出现精度问题时可以尝试关闭数据缓存优化。如果关闭数据缓存优化功能后，精度达标，则需要识别出问题算子，反馈给技术支持进一步分析、解决算子问题；解决算子问题后，建议仍旧保持开启数据缓存优化功能。

**配置示例：**

```c++
{"ge.bufferOptimize", "l2_optimize"};
```

**必选/可选**：可选

**生效级别**：session/graph

## ge.exec.enableDump

是否开启dump功能。

- 1：开启dump功能，从dump\_path读取dump文件保存路径，dump\_path为None时会产生异常。
- 0：（默认值）关闭dump功能。

**配置示例：**

```c++
{"ge.exec.enableDump", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session

**使用约束：**

- 该参数与ge.exec.enableDumpDebug参数，全局场景以及同一个Session内场景，不能同时设置。
- 若“ge.exec.enableDump/ge.exec.enableDumpDebug（二选一）”参数配置为“1”，同时“ge.exec.enable\_exception\_dump”配置为“1”（即开启普通ExceptionDump），此时：
  - 针对动态shape网络，仅“ge.exec.enable\_exception\_dump”生效。
  - 针对静态shape网络，“ge.exec.enable\_exception\_dump”与“ge.exec.enableDump/ge.exec.enableDumpDebug（二选一）”都生效。

## ge.exec.enableDumpDebug

是否开启溢出检测功能。

- 1：开启溢出检测功能，从ge.exec.dumpPath读取Dump文件保存路径，路径配置为None时会产生异常。
- 0：（默认值）关闭溢出检测功能。

**配置示例：**

```c++
{"ge.exec.enableDumpDebug", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session

**使用约束：**

- 该参数与ge.exec.enableDump参数，全局场景以及同一个Session内场景，不能同时设置。
- 若“ge.exec.enableDump/ge.exec.enableDumpDebug（二选一）”参数配置为“1”，同时“ge.exec.enable\_exception\_dump”配置为“1”（即开启普通ExceptionDump），此时：
  - 针对动态shape网络，仅“ge.exec.enable\_exception\_dump”生效。
  - 针对静态shape网络，“ge.exec.enable\_exception\_dump”与“ge.exec.enableDump/ge.exec.enableDumpDebug（二选一）”都生效。

## ge.exec.dumpData

指定算子dump内容类型，取值：

- tensor：（默认值）dump算子数据。
- stats：dump算子统计数据，保存结果为csv格式。一般情况下dump算子数据量大，用户可以尝试dump算子统计数据。

**配置示例：**

```c++
{"ge.exec.dumpData", "tensor"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.exec.dumpDebugMode

溢出检测模式。

**参数取值：**

- aicore\_overflow：AI Core算子溢出检测，检测在算子输入数据正常的情况下，输出是否不正常的极大值（如float16下65500,38400,51200这些值）。一旦检测出这类问题，需要根据网络实际需求和算子逻辑来分析溢出原因并修改算子实现。
- atomic\_overflow：Atomic Add溢出检测，即除了AICore之外，还有其他涉及浮点计算的模块，比如SDMA，检测这些部分出现的溢出问题。
- all：同时进行AI Core算子溢出检测和Atomic Add溢出检测。

<!-- npu="A3,910b" id1 -->
针对如下产品，仅支持配置为“all”：

<!-- npu="910b" id2 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id2 -->

<!-- npu="A3" id3 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id3 -->
<!-- end id1 -->

**配置示例：**

```c++
{"ge.exec.dumpDebugMode", "all"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.exec.dumpLayer

指定需要dump的算子，取值为算子名，多个算子名之间使用空格分隔。

若指定的算子其输入涉及data算子，会同时将data算子信息dump出来。

**配置示例**：

```c++
{"ge.exec.dumpLayer", "layer1 layer2 layer3"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.exec.dumpMode

dump模式，用于指定dump算子输入数据还是输出数据。取值如下：

- input：仅dump算子输入数据。
- output：（默认值）仅dump算子输出数据。
- all：dump算子输入和输出数据。

**配置示例：**

```c++
{"ge.exec.dumpMode", "input"};
```

**使用约束：**

配置为all时，由于部分算子在执行过程中会修改输入数据，例如集合通信类算子HcomAllGather、HcomAllReduce等，因此系统在进行dump时，会在算子执行前dump算子输入，在算子执行后dump算子输出，这样，针对同一个算子，算子输入、输出的dump数据是分开落盘，会出现多个dump文件，在解析dump文件后，用户可通过文件内容判断是输入还是输出。

**必选/可选**：可选

**生效级别**：全局/session

## ge.exec.dumpPath

Dump文件保存路径。开启dump和溢出检测功能时，该参数必须配置。

该参数指定的目录需要在启动训练的环境上（容器或Host侧）提前创建且确保安装时配置的运行用户具有读写权限，支持配置绝对路径或相对路径（相对执行命令行时的当前路径）。

- 绝对路径配置以“/”开头，例如：/home/HwHiAiUser/output。
- 相对路径配置直接以目录名开始，例如：output。

dump文件生成在该参数指定的目录下，即\{dump\_path\}/\{time\}/\{deviceid\}/\{model\_name\}/\{model\_id\}/\{data\_index\}目录下，以\{dump\_path\}配置/home/HwHiAiUser/output为例，例如存放在“/home/HwHiAiUser/output/20200808163566/0/ge\_default\_20200808163719\_121/11/0”目录下。

**必选/可选**：可选

**生效级别**：全局/session

## ge.exec.dumpStep

指定采集哪些迭代的dump数据。默认值：None，表示所有迭代都会产生dump数据。

多个迭代用“|”分隔，例如：0|5|10；也可以用"-"指定迭代范围，例如：0|3-5|10。

**配置示例：**

```c++
{"ge.exec.dumpStep", "None"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.fusionSwitchFile

融合规则（包括图融合和UB融合）开关配置文件路径以及文件名，路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符。

系统内置了一些图融合和UB融合规则，均为默认开启，可以根据需要关闭指定的融合规则，当前可以关闭的融合规则请参见《图融合和UB融合规则参考》，但是由于系统机制，其他融合规则无法关闭。

配置文件样例_fusion\_switch.cfg_如下所示_，_on表示开启，off表示关闭。

配置文件样例：

```json
{
    "Switch":{
        "GraphFusion":{
            "RequantFusionPass":"on",
            "ConvToFullyConnectionFusionPass":"off",
            "SoftmaxFusionPass":"on",
            "NotRequantFusionPass":"on",
            "SplitConvConcatFusionPass":"on",
            "ConvConcatFusionPass":"on",
            "MatMulBiasAddFusionPass":"on",
            "PoolingFusionPass":"on",
            "ZConcatv2dFusionPass":"on",
            "ZConcatExt2FusionPass":"on",
            "TfMergeSubFusionPass":"on"
        },
        "UBFusion":{
            "TbePool2dQuantFusionPass":"on"
        }
    }
}
```
<!-- npu="950" id4 -->
**Ascend 950PR/Ascend 950DT不支持UB融合，可以不配置UBFusion。**
<!-- end id4 -->

同时支持用户一键关闭融合规则：

配置文件样例：

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
<!-- npu="950" id5 -->
**Ascend 950PR/Ascend 950DT不支持UB融合，可以不配置UBFusion。**
<!-- end id5 -->

需要注意的是：

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

**配置示例：**

```c++
{"ge.fusionSwitchFile", "/home/test/fusion_switch.cfg"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph
