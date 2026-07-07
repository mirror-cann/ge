# 量化模型时校准集数据大小与模型输入大小不匹配，量化模型失败

## 问题现象描述

**执行ATC模型转换命令时，通过--compression\_optimize\_conf参数配置模型量化（将模型中的权重由浮点数float32量化到低比特整数int8）相关的选项**，结果报错提示如下：

```console
[ERROR] AMCT(21177,atc.bin):2023-04-14-14:43:17[utils_acl.cpp:133]input image size[1579014] is not equal to model input size[3158028]
[ERROR] AMCT(21177,atc.bin):2023-04-14-14:43:17[sample_process.cpp:234]memcpy device buffer failed
[ERROR] AMCT(21177,atc.bin):2023-04-14-14:43:17[sample_process.cpp:298]execute PreProcess failed
[ERROR] AMCT(21177,atc.bin):2023-04-14-14:43:17[sample_process.cpp:275]ACL model infer failed.
[ERROR] AMCT(21177,atc.bin):2023-04-14-14:43:17[quantize_api.cpp:240]sample process failed
[ERROR] AMCT(21177,atc.bin):2023-04-14-14:43:17[quantize_api.cpp:376]Do Calibration failed.
[ERROR] AMCT(21177,atc.bin):2023-04-14-14:43:20[inner_graph_calibration.cpp:78]Failed to execute InnerQuantizeGraph failed.
...
ATC run failed, Please check the detail log, Try 'atc --help' for more information
```

## 原因分析

通过报错提示**input image size\[xxxxxx\] is not equal to model input size\[xxxxxx\]**可知，量化模型时校准集数据大小与模型输入大小不匹配，不匹配可能是校准集数据的shape与模型输入shape不一致，也有可能是校准集数据的数据类型与模型输入数据类型不一致。

## 解决措施

需要逐一排查校准集数据shape、数据类型与模型输入shape、数据类型是否一致。

量化时，模型输入shape通过量化配置文件中的input\_shape参数配置，模型输入数据类型需由用户从获取模型的网站获取或通过第三方软件打开模型文件查看。
