# --display\_model\_info

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
全量芯片支持
<!-- end id2 -->

<!-- npu="IPV350" id1 -->
- IPV350：不支持
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--display_model_info_res.md#id1 -->

## 功能说明

编译原始框架网络模型时，查询模型占用的关键资源信息、编译与运行环境等信息，查询出的信息直接在屏幕打印显示。

## 参数取值

**参数值：**

- 0：（默认值）关闭查询功能。
- 1：打开查询功能。

**参数值约束：**
该参数不支持单算子描述文件转离线模型时信息的查看，即不支持与[--singleop](--singleop.md)参数同时使用。

## 推荐配置及收益

无。

## 示例

下面示例以TensorFlow框架网络模型为例进行说明：

```bash
atc --model=$HOME/module/resnet50_tensorflow.pb  --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version> --display_model_info=1 ...
```

命令执行完毕，屏幕会打印类似如下信息：

```console
============ Display Model Info start ============
# 模型转换使用的atc命令
Original Atc command line: ${INSTALL_DIR}/bin/atc.bin --model=$HOME/module/resnet50_tensorflow.pb  --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version> --display_model_info=1
# ATC软件版本信息、soc_version版本信息、原始框架信息
system   info: atc_version[xxx], soc_version[xxx], framework_type[xxx].
# 运行时的占用内存、权重大小、逻辑stream数目、event数目
resource info: memory_size[xxx B], weight_size[xxx B], stream_num[xxx], event_num[xxx].
# 离线模型文件中各分区大小、包括ModelDef、权重、tbe_kernels、task_info、so占用的大小等
om       info: modeldef_size[xxx B], weight_data_size[xxx B], tbe_kernels_size[xxx B], cust_aicpu_kernel_store_size[xxx B], task_info_size[xxx B], so_store_size[xxx B].
============ Display Model Info end   ============
```

## 依赖约束

无。
