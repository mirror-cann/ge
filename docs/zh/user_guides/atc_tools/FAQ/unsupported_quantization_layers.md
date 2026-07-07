# 模型中存在不支持量化的层，量化模型失败

## 问题现象描述

**执行ATC模型转换命令时，通过--compression\_optimize\_conf参数配置模型量化（将模型中的权重由浮点数float32量化到低比特整数int8）相关的选项**，结果报错提示如下：

```console
ATC start working now, please wait for a moment.
[ERROR][ProcessScale][52] Not support scale greater than 1 / FLT_EPSILON.
[ERROR][WtsArqCalibrationCpuKernel][188] ArqQuantCPU scale is illegal.
[ERROR][ArqQuant][301] WtsArqCalibrationCpuKernel of format CO_CI_KH_KW failed.
[ERROR] AMCT(14815,atc.bin):2023-04-14-12:23:19[weight_algorithm.cpp:137]Default/network-DeepLabV3/resnet-Resnet/layer4-SequentialCell/0-Bottleneck/downsample-SequentialCell/0-Conv2d/Conv2D-op311 arq weight fake quant failed!
[ERROR] AMCT(14815,atc.bin):2023-04-14-12:23:19[weight_calibration_pass.cpp:90]Fail to execute WeightFakeQuant without trans!
[ERROR] AMCT(14815,atc.bin):2023-04-14-12:23:19[weight_calibration_pass.cpp:185]layer Default/network-DeepLabV3/resnet-Resnet/layer4-SequentialCell/0-Bottleneck/downsample-SequentialCell/0-Conv2d/Conv2D-op311 run WeightFakeQuantArq failed
[ERROR] AMCT(14815,atc.bin):2023-04-14-12:23:19[graph_optimizer.cpp:43]pass run failed
[ERROR] AMCT(14815,atc.bin):2023-04-14-12:23:19[quantize_api.cpp:227]Do GenerateCalibrationGraph optimizer pass failed.
[ERROR] AMCT(14815,atc.bin):2023-04-14-12:23:19[quantize_api.cpp:363]Generate calibration Graph failed.
[ERROR] AMCT(14815,atc.bin):2023-04-14-12:23:22[inner_graph_calibration.cpp:78]Failed to execute InnerQuantizeGraph failed.
```

## 原因分析

通过报错提示**layer  _xxxxxx_  run WeightFakeQuantArq failed**可知，当前模型中有权重相关的层不支持量化，需要跳过这些不支持量化的层。

## 解决措施

跳过不支持量化的层，配置方法如下：

1. 增加配置，跳过不支持量化的层。

    新增一个配置文件，文件名后缀为.cfg，例如_simple\_config.cfg_，文件内容如下，加粗部分为报错提示中不支持量化的层：

    ```ini
    skip_layers: "Default/network-DeepLabV3/resnet-Resnet/layer4-SequentialCell/0-Bottleneck/downsample-SequentialCell/0-Conv2d/Conv2D-op311"
    ```

    同时，在--compression\_optimize\_conf参数指定的量化配置文件中，增加config\_file参数：

    ```textproto
    calibration:
    {
        input_data_dir: xxxxxx
        config_file: simple_config.cfg
        input_shape: xxxxxx
        infer_soc: xxxxxx
    }
    ```

2. 重新执行模型转换。
3. 重新执行推理。

    如果跳过不支持量化的层影响模型推理的结果数据，则需要用户自行调整模型，再重新量化模型。
