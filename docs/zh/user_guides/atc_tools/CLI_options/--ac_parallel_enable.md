# --ac\_parallel\_enable

## 产品支持情况

<!-- npu="950" id2 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id2 -->

<!-- npu="A3" id3 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id3 -->

<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id4 -->

<!-- npu="310b" id5 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id5 -->

<!-- npu="310p" id6 -->
- Atlas 推理系列产品：支持
<!-- end id6 -->

<!-- npu="910" id7 -->
- Atlas 训练系列产品：支持
<!-- end id7 -->

<!-- npu="IPV350" id1 -->
- IPV350：不支持
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--ac_parallel_enable_res.md#id1 -->

## 功能说明

<!-- npu="950,A3,910b,910,310p,310b,IPV350" id8 -->
动态shape图中，是否允许AI CPU算子和AI Core算子并行运行。

动态shape图中，开关开启时，系统自动识别图中可以和AI Core并发的AI CPU算子，不同引擎的算子下发到不同流上，实现多引擎间的并行，从而提升资源利用效率和动态shape执行性能。
<!-- end id8 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--ac_parallel_enable_res.md#id2 -->

## 参数取值

<!-- npu="950,A3,910b,910,310p,310b,IPV350" id9 -->
- 1：允许AI CPU和AI Core算子间的并行运行。
<!-- end id9 -->
<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--ac_parallel_enable_res.md#id3 -->
- 0：（默认值）AI CPU算子不会单独分流。

## 推荐配置及收益

无。

## 示例

```bash
atc --ac_parallel_enable=1 ...
```
