# --singleop

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id15 -->
全量芯片支持
<!-- end id15 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--singleop_res.md#id1 -->

<!-- npu="IPV350" id1 -->
IPV350：不支持
<!-- end id1 -->

## 功能说明

单算子描述文件，将单个算子描述文件（JSON格式）转换成适配AI处理器的离线模型，以便进行后续的单算子功能验证。

兼容性说明：

动态shape算子场景和昇腾虚拟化实例场景，om离线模型转换环境的CANN软件包版本，必须与运行环境的CANN软件包版本相同。

（昇腾虚拟化实例主要是指NPU资源虚拟化的场景，其主要目的是为了通过相互隔离的vNPU虚拟实例提升物理NPU资源利用率。）

昇腾虚拟化实例支持情况如下：

<!-- npu="950" id5 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id5 -->

<!-- npu="A3" id6 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id6 -->

<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id4 -->

<!-- npu="310b" id7 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id7 -->

<!-- npu="310p" id2 -->
- Atlas 推理系列产品：支持
<!-- end id2 -->

<!-- npu="910" id3 -->
- Atlas 训练系列产品：支持
<!-- end id3 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--singleop_res.md#id2 -->

<!-- npu="IPV350" id8 -->
- IPV350：不支持
<!-- end id8 -->

## 关联参数

使用该参数时，只有如下参数可以配合使用，其中[--output](--output.md)、[--soc\_version](--soc_version.md)为必填。

<!-- npu="950" id14 -->
- Ascend 950PR/Ascend 950DT可配合使用的参数：
  - [--output](--output.md)
  - [--soc\_version](--soc_version.md)
  - [--precision\_mode](--precision_mode.md)
  - [--precision\_mode\_v2](--precision_mode_v2.md)
  - [--op\_select\_implmode](--op_select_implmode.md)
  - [--optypelist\_for\_implmode](--optypelist_for_implmode.md)
  - [--enable\_small\_channel](--enable_small_channel.md)
  - [--sparsity](--sparsity.md)
  - [--allow\_hf32](--allow_hf32.md)
  - [--log](--log.md)
  - [--debug\_dir](--debug_dir.md)
  - [--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)
  - [--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)
  - [--atomic\_clean\_policy](--atomic_clean_policy.md)
  - [--customize\_dtypes](--customize_dtypes.md)
  - [--is\_weight\_clip](--is_weight_clip.md)
<!-- end id14 -->

<!-- npu="A3" id13 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品可配合使用的参数：
  - [--output](--output.md)
  - [--soc\_version](--soc_version.md)
  - [--precision\_mode](--precision_mode.md)
  - [--precision\_mode\_v2](--precision_mode_v2.md)
  - [--op\_select\_implmode](--op_select_implmode.md)
  - [--optypelist\_for\_implmode](--optypelist_for_implmode.md)
  - [--enable\_small\_channel](--enable_small_channel.md)
  - [--sparsity](--sparsity.md)
  - [--allow\_hf32](--allow_hf32.md)
  - [--log](--log.md)
  - [--debug\_dir](--debug_dir.md)
  - [--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)
  - [--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)
  - [--atomic\_clean\_policy](--atomic_clean_policy.md)
  - [--customize\_dtypes](--customize_dtypes.md)
  - [--is\_weight\_clip](--is_weight_clip.md)
<!-- end id13 -->

<!-- npu="910b" id12 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品可配合使用的参数：
  - [--output](--output.md)
  - [--soc\_version](--soc_version.md)
  - [--precision\_mode](--precision_mode.md)
  - [--precision\_mode\_v2](--precision_mode_v2.md)
  - [--op\_select\_implmode](--op_select_implmode.md)
  - [--optypelist\_for\_implmode](--optypelist_for_implmode.md)
  - [--enable\_small\_channel](--enable_small_channel.md)
  - [--sparsity](--sparsity.md)
  - [--allow\_hf32](--allow_hf32.md)
  - [--log](--log.md)
  - [--debug\_dir](--debug_dir.md)
  - [--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)
  - [--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)
  - [--atomic\_clean\_policy](--atomic_clean_policy.md)
  - [--customize\_dtypes](--customize_dtypes.md)
  - [--is\_weight\_clip](--is_weight_clip.md)
<!-- end id12 -->

<!-- npu="310b" id9 -->
- Atlas 200I/500 A2 推理产品可配合使用的参数：
  - [--output](--output.md)
  - [--soc\_version](--soc_version.md)
  - [--precision\_mode](--precision_mode.md)
  - [--precision\_mode\_v2](--precision_mode_v2.md)
  - [--op\_select\_implmode](--op_select_implmode.md)
  - [--optypelist\_for\_implmode](--optypelist_for_implmode.md)
  - [--enable\_small\_channel](--enable_small_channel.md)
  - [--sparsity](--sparsity.md)
  - [--log](--log.md)
  - [--debug\_dir](--debug_dir.md)
  - [--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)
  - [--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)
  - [--atomic\_clean\_policy](--atomic_clean_policy.md)
  - [--customize\_dtypes](--customize_dtypes.md)
  - [--is\_weight\_clip](--is_weight_clip.md)
<!-- end id9 -->

<!-- npu="310p" id10 -->
- Atlas 推理系列产品可配合使用的参数：
  - [--output](--output.md)
  - [--soc\_version](--soc_version.md)
  - [--core\_type](--core_type.md)
  - [--aicore\_num](--aicore_num.md)
  - [--precision\_mode](--precision_mode.md)
  - [--precision\_mode\_v2](--precision_mode_v2.md)
  - [--op\_select\_implmode](--op_select_implmode.md)
  - [--optypelist\_for\_implmode](--optypelist_for_implmode.md)
  - [--log](--log.md)
  - [--debug\_dir](--debug_dir.md)
  - [--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)
  - [--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)
  - [--atomic\_clean\_policy](--atomic_clean_policy.md)
  - [--customize\_dtypes](--customize_dtypes.md)
  - [--is\_weight\_clip](--is_weight_clip.md)
<!-- end id10 -->

<!-- npu="910" id11 -->
- Atlas 训练系列产品可配合使用的参数：
  - [--output](--output.md)
  - [--soc\_version](--soc_version.md)
  - [--precision\_mode](--precision_mode.md)
  - [--precision\_mode\_v2](--precision_mode_v2.md)
  - [--op\_select\_implmode](--op_select_implmode.md)
  - [--optypelist\_for\_implmode](--optypelist_for_implmode.md)
  - [--enable\_small\_channel](--enable_small_channel.md)
  - [--log](--log.md)
  - [--debug\_dir](--debug_dir.md)
  - [--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)
  - [--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)
  - [--atomic\_clean\_policy](--atomic_clean_policy.md)
  - [--customize\_dtypes](--customize_dtypes.md)
  - [--is\_weight\_clip](--is_weight_clip.md)
<!-- end id11 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--singleop_res.md#id3 -->

## 参数取值

**参数值：**
单算子描述文件（JSON格式）格式以及参数配置请参见[单算子模型转换](../sinlgeop_model_convert/sinlgeop_model_convert.md)。

**参数值约束：**
该参数指定的单算子都是基于Ascend IR定义的，关于单算子的详细定义请参见[《算子库》](https://hiascend.com/document/redirect/CannCommunityOplist) 中的“Ascend IR算子规格说明”章节。

## 推荐配置及收益

无。

## 示例

下面以Add单算子为例进行说明，该单算子对应的描述文件为_add.json_  ，将该文件上传到ATC工具所在服务器任意目录，例如上传到$HOME/singleop，使用示例如下：

```bash
atc --singleop=$HOME/singleop/add.json --output=$HOME/singleop/out/op_model  --soc_version=<soc_version> ...
```

## 依赖约束

- 使用约束

    单算子JSON文件转换成离线模型场景，如果希望模型转换时只使用TBE算子（不查找AI CPU算子，找不到TBE算子则报错），还需设置如下环境变量：

    ```bash
    export ASCEND_ENGINE_PATH=${INSTALL_DIR}/lib64/plugin/opskernel/libfe.so:${INSTALL_DIR}/lib64/plugin/opskernel/libge_local_engine.so:${INSTALL_DIR}/lib64/plugin/opskernel/librts_engine.so
    ```

    执行上述命令后，如果用户想要执行其他操作，需要删除上述环境变量：执行**unset ASCEND\_ENGINE\_PATH**命令，使其失效。

    其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

- 接口约束

    单算子描述文件转换后的om离线模型文件，使用应用工程进行模型推理时，需调用单算子模型加载接口加载算子模型（例如[aclopSetModelDir](../../../api/graph_engine_api/c/acl/aclopSetModelDir.md)接口），最后调用单算子执行接口执行算子（例如[aclopExecuteV2](../../../api/graph_engine_api/c/acl/aclopExecuteV2.md)接口）。
