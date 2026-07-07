# 量化模型时模型输入大小过大，AI Core执行任务失败，量化模型失败

## 问题现象描述

模型转换命令示例如下，模型输入大小与input\_shape参数指定的shape有关：

```bash
atc --model=xxxxxx.pb --framework=3 --output=xxxxxx --soc_version=xxxxxx --input_shape="input:64,224,224,3" --input_format=NHWC --compression_optimize_conf=config/quant.cfg
```

模型转换时，报错示例如下：

```console
[ERROR] AMCT(757013,atc.bin):2023-03-14-14:15:54[model_process.cpp:299]execute model failed, modelId is 1, errorCode is 507011
[ERROR] AMCT(757013,atc.bin):2023-03-14-14:15:54[sample_process.cpp:320]execute inference failed
[ERROR] AMCT(757013,atc.bin):2023-03-14-14:15:55[sample_process.cpp:275]ACL model infer failed.
[ERROR] AMCT(757013,atc.bin):2023-03-14-14:15:55[quantize_api.cpp:242]sample process failed
[ERROR] AMCT(757013,atc.bin):2023-03-14-14:15:55[quantize_api.cpp:378]Do Calibration failed.
[ERROR] AMCT(757013,atc.bin):2023-03-14-14:15:56[inner_graph_calibration.cpp:77]Failed to execute InnerQuantizeGraph failed.
ATC run failed, Please check the detail log, Try 'atc --help' for more information
EZ9999: Inner Error!
EZ9999  Aicore kernel execute failed, device_id=0, stream_id=11, report_stream_id=2, task_id=209, flip_num=0, fault kernel_name=17786444594805609729-1_0_1_vgg_16/conv1/conv1_2/Conv2D_histo, program id=206, hash=1846532111878224358.[FUNC:GetError][FILE:stream.cc][LINE:1131]
        TraceBack (most recent call last):
        Model synchronize execute failed, model_id=1![FUNC:GetStreamToSyncExecute][FILE:model.cc][LINE:630]
        rtModelExecute execute failed, reason=[the model stream execute failed][FUNC:FuncErrorReason][FILE:error_message_manage.cc][LINE:49]
        [Exec][Model]Execute model failed, ge result[507011], modelId[1][FUNC:ReportCallError][FILE:log_inner.cpp][LINE:161]
        [Exec][Model]modelId[1] execute failed, result[507011][FUNC:ReportInnerError][FILE:log_inner.cpp][LINE:145]
        An unknown error occurred. Please check the log.
```

## 原因分析

AI Core执行任务失败，猜测可能是因为input\_shape参数处的Batch size值过大，导致AI Core上的算子执行失败。

## 解决措施

可将input\_shape参数处的Batch size值调小，例如：--input\_shape="**input:8,224,224,3**"，调整参数值之后，再重新转换模型。
