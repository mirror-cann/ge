# 精度比对

## BUFFER\_OPTIMIZE

数据缓存优化开关。此参数实际对应的options参数为`ge.bufferOptimize`。

**参数取值：**

- l1\_optimize：表示开启l1优化。当前版本该参数无效，等同于off\_optimize。
- l2\_optimize：（默认值）表示开启l2优化。
- off\_optimize：表示关闭数据缓存优化。

其中，l1表示L1 Buffer，通用内部存储，是AI Core内比较大的一块数据中转区，可暂存AI Core中需要反复使用的一些数据从而减少从总线读写的次数；l2表示L2 Buffer，表示外部存储；AI Core需要把外部存储中的数据加载到内部存储中，才能完成相应的计算。

**使用建议：**

建议打开数据缓存优化功能：开启数据缓存优化可提高计算效率、提升性能，但由于部分算子在实现上可能存在未考虑的场景，导致影响精度，因此在出现精度问题时可以尝试关闭数据缓存优化。如果关闭数据缓存优化功能后，精度达标，则需要识别出问题算子，反馈给技术支持进一步分析、解决算子问题；解决算子问题后，建议仍旧保持开启数据缓存优化功能。

**配置示例：**

```c++
{ge::ir_option::BUFFER_OPTIMIZE, "l2_optimize"}
```

**产品支持情况：**

全量芯片支持。

## FUSION\_SWITCH\_FILE

此参数实际对应的options参数为`ge.fusionSwitchFile`。

融合开关配置文件路径以及文件名，路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符。

系统内置了一些图融合和UB融合规则，均为默认开启，可以根据需要关闭指定的融合规则，当前可以关闭的融合规则请参见《图融合和UB融合规则参考》，但是由于系统机制，其他融合规则无法关闭。

<!-- npu="950" id1 -->
**Ascend 950PR/Ascend 950DT不支持UB融合**。
<!-- end id1 -->

**配置示例：**

配置文件样例_fusion\_switch.cfg，_on表示开启，off表示关闭。

配置样例：

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
        }
    }
}
```

<!-- npu="950" id2 -->
**不支持UB融合时，上述配置文件可以删除UBFusion。**
<!-- end id2 -->

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

<!-- npu="950" id3 -->
**不支持UB融合时，上述配置文件可以删除UBFusion。**
<!-- end id3 -->

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

    <!-- npu="950" id4 -->
    **不支持UB融合时，上述配置文件可以删除UBFusion。**
    <!-- end id4 -->

**产品支持情况：**

全量芯片支持。
