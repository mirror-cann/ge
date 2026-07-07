# 量化简易配置文件

如果要自动控制量化过程，例如控制哪些层是否量化、控制使用什么量化算法，则可以通过本章节构造的cfg配置文件实现。

## 配置文件参数

配置文件支持的消息以及对应参数说明如下：

- AMCTConfig：AMCT训练后量化的简易配置。

    | 是否必填 | 类型 | 字段 | 说明 |
    | --- | --- | --- | --- |
    | optional | bool | activation_offset | 数据量化是否带offset。全局配置参数。<br><br>  - 带offset：（默认值）数据量化使用非对称量化。<br>  - 不带offset：数据量化使用对称量化。 |
    | optional | bool | joint_quant | 是否进行Eltwise联合量化，默认为false，表示关闭联合量化功能。<br>开启后对部分网络可能会存在性能提升但是精度下降的问题。 |
    | repeated | string | skip_layers | 不需要量化层的层名。 |
    | repeated | string | skip_layer_types | 不需要量化的层类型。 |
    | optional | int32 | version | 简易配置文件的版本。 |
    | optional | CalibrationConfig | common_config | 通用的量化配置，全局量化配置参数。若某层未被override_layer_types或者override_layer_configs重写，则使用该配置。<br>参数优先级：override_layer_configs>override_layer_types>common_config |
    | repeated | OverrideLayerType | override_layer_types | 重写某一类型层的量化配置，即对哪些层进行差异化量化。<br>例如全局量化配置参数配置的量化因子搜索步长为0.01，可以通过该参数对部分层进行差异化量化，可以配置搜索步长为0.02。<br>参数优先级：override_layer_configs>override_layer_types>common_config |
    | repeated | OverrideLayer | override_layer_configs | 重写某一层的量化配置，即对哪些层进行差异化量化。<br>例如全局量化配置参数配置的量化因子搜索步长为0.01，可以通过该参数对部分层进行差异化量化，可以配置搜索步长为0.02。<br>参数优先级：override_layer_configs>override_layer_types>common_config |
    | optional | bool | do_fusion | 是否开启BN融合功能，默认为true，表示开启该功能。 |
    | repeated | string | skip_fusion_layers | 跳过BN融合的层，配置之后这些层不会进行BN融合。 |
    | repeated | TensorQuantize | tensor_quantize | 对网络模型中指定节点的输入Tensor进行训练后量化，来提高数据搬运时的推理性能。<br>当前仅支持对MaxPool/Add/Eltwise算子做tensor量化。 |

- OverrideLayerType：支持量化的层类型的名称。

    | 是否必填 | 类型 | 字段 | 说明 |
    | --- | --- | --- | --- |
    | required | string | layer_type | 支持量化的层类型的名称。 |
    | required | CalibrationConfig | calibration_config | 重置的量化配置。 |

- OverrideLayer：重置某层量化配置。

    | 是否必填 | 类型 | 字段 | 说明 |
    | --- | --- | --- | --- |
    | required | string | layer_name | 被重置层的层名。 |
    | required | CalibrationConfig | calibration_config | 重置的量化配置。 |

- CalibrationConfig：Calibration量化的配置。

    | 是否必填 | 类型 | 字段 | 说明 |
    | --- | --- | --- | --- |
    | - | ARQuantize | arq_quantize | 权重量化算法配置。<br>arq_quantize：ARQ量化算法配置。 |
    | - | FMRQuantize | ifmr_quantize | 数据量化算法配置。<br>ifmr_quantize：IFMR量化算法配置。 |
    | optional | bool | weight_compress_only | 是否只进行权重量化。仅权重量化场景，支持的数据类型必须为Float32，Float16。<br><br>  - true：只进行权重量化。<br>  - false：权重和数据都量化。默认为false。 |

- ARQuantize：ARQ权重量化算法配置。

    | 是否必填 | 类型 | 字段 | 说明 |
    | --- | --- | --- | --- |
    | optional | bool | channel_wise | 是否对每个channel采用不同的量化因子。<br><br>  - true：每个channel独立量化，量化因子不同。<br>  - false：所有channel同时量化，共享量化因子。 |
    | optional | bool | asymmetric | 是否对权重进行非对称量化。用于控制逐层量化算法的选择。<br>只在weight_compress_only为true时生效，若weight_compress_only设置为false，则asymmetric只能设置为false。<br><br>  - true：权重量化使用非对称量化（offset不为0）。<br>  - false：权重量化使用对称量化（offset为0），默认为false。<br><br>如果override_layer_configs、override_layer_types、common_config配置项都配置该参数，则生效优先级为：<br>override_layer_configs>override_layer_types>common_config |
    | optional | uint32 | quant_bits | 权重量化位宽。支持配置为INT6、INT7、INT8，默认为INT8量化。<br>该字段配置为INT6、INT7仅支持Conv2d类型算子。<br>如果在common_config中配置quant_bits为INT6、INT7，则只对Conv2d算子生效，其他算子改为默认INT8。<br>针对ONNX网络模型，如果在override_layer_types中指定Conv类算子quant_bits为INT6、INT7，则只对weight dim为4场景生效。 |

- FMRQuantize：FMR数据量化算法配置。

    | 是否必填 | 类型 | 字段 | 说明 |
    | --- | --- | --- | --- |
    | optional | float | search_range_start | 量化因子搜索范围左边界。 |
    | optional | float | search_range_end | 量化因子搜索范围右边界。 |
    | optional | float | search_step | 量化因子搜索步长。 |
    | optional | float | max_percentile | 最大值搜索位置。 |
    | optional | float | min_percentile | 最小值搜索位置。 |
    | optional | bool | asymmetric | 是否对数据进行非对称量化。用于控制逐层量化算法的选择。<br><br>  - true：非对称量化<br>  - false：对称量化<br><br>如果override_layer_configs、override_layer_types、common_config配置项都配置该参数，或者配置了<br>activation_offset参数，则生效优先级为：<br>override_layer_configs>override_layer_types>common_config>activation_offset |
    | optional | CalibrationDataType | dst_type | 量化位宽，数据量化是采用INT8量化还是INT16量化，默认为INT8量化。<br>当前版本仅支持INT8量化。 |

- TensorQuantize：需要进行训练后量化的输入Tensor配置。

    | 是否必填 | 类型 | 字段 | 说明 |
    | --- | --- | --- | --- |
    | required | string | layer_name | 需要对节点输入Tensor进行训练后量化的节点名称，当前仅支持对MaxPool算子的输入Tensor进行量化。 |
    | required | uint32 | input_index | 需要对节点输入Tensor进行训练后量化的节点的输入索引。 |
    | - | FMRQuantize | ifmr_quantize | 数据量化算法配置。<br>ifmr_quantize：IFMR量化算法配置。默认为IFMR量化算法。 |

## 配置样例

- 基于该文件构造的**均匀量化简易配置文件**_quant_.cfg样例如下所示：_Optype_需要配置为基于Ascend IR定义的算子类型，详细对应关系请参见[支持量化的层及约束](quantization_layers_constraints.md)。

    ```textproto
    # global quantize parameter
    activation_offset : true
    joint_quant : false
    enable_auto_nuq : false
    version : 1
    skip_layers : "Optype"
    skip_layer_types: "Optype"
    do_fusion: true
    skip_fusion_layers : "Optype"
    common_config : {
        arq_quantize : {
            channel_wise : true
            quant_bits : 7

        }
        ifmr_quantize : {
            search_range_start : 0.7
            search_range_end : 1.3
            search_step : 0.01
            max_percentile : 0.999999
            min_percentile : 0.999999
            asymmetric : true
        }
    }

    override_layer_types : {
        layer_type : "Optype"
        calibration_config : {
            arq_quantize : {
                channel_wise : false
            }
            ifmr_quantize : {
                search_range_start : 0.8
                search_range_end : 1.2
                search_step : 0.02
                max_percentile : 0.999999
                min_percentile : 0.999999
                asymmetric : false
            }
        }
    }

    override_layer_configs : {
        layer_name : "Opname"
        calibration_config : {
            arq_quantize : {
                channel_wise : true
            }
            ifmr_quantize : {
                search_range_start : 0.8
                search_range_end : 1.2
                search_step : 0.02
                max_percentile : 0.999999
                min_percentile : 0.999999
                asymmetric : false
            }
        }
    }
    tensor_quantize {
        layer_name: "Opname"
        input_index: 0
        ifmr_quantize: {
            search_range_start : 0.7
            search_range_end : 1.3
            search_step : 0.01
            min_percentile : 0.999999
            asymmetric : false
           }
    }
    tensor_quantize {
        layer_name: "Opname"
        input_index: 0
    }
    ```

- 基于该文件构造的**仅权重量化简易配置文件**_quant_.cfg配置示例：

    ```textproto
    activation_offset : true
    joint_quant : false
    version : 1
    do_fusion: true
    common_config : {
       weight_compress_only : true
        arq_quantize : {
            channel_wise : true
            asymmetric : false

        }
    }

    override_layer_types : {
        layer_type : "Optype"
        calibration_config : {
            weight_compress_only : true
            arq_quantize : {
                channel_wise : true
                asymmetric : true
                quant_bits : 6
            }
        }
    }

    override_layer_configs : {
        layer_name : "Opname"
        calibration_config : {
            weight_compress_only : true
            arq_quantize : {
                channel_wise : true
                asymmetric : true
            }
        }
    }
    ```
