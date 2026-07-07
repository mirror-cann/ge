# 开启量化功能，模型转换时提示“build\_main build graph\[infer\_graph\_info\] failed”

## 问题现象描述

模型转换时，通过[--compression\_optimize\_conf](../CLI_options/--compression_optimize_conf.md)参数配置了量化功能，结果模型转换失败，提示信息如下：

![图示](../figures/quant_build_error.png)

## 原因分析

该问题可能是原始模型编译失败。

## 解决措施

可以尝试将量化功能关闭，重新进行模型转换，检查出错原因；待原始模型不开启量化功能模型转换成功后，再尝试启用量化功能。
