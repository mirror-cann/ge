# --op\_name\_map

## 产品支持情况

<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- npu="950" id6 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id6 -->
<!-- npu="A3" id5 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id5 -->
<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id4 -->
<!-- npu="310b" id3 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id3 -->
<!-- npu="310p" id2 -->
- Atlas 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910" id1 -->
- Atlas 训练系列产品：支持
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--op_name_map_res.md#id1 -->

## 功能说明

扩展算子（非标准算子）映射配置文件路径和文件名，不同的网络中某扩展算子的功能不同，可以指定该扩展算子到具体网络中实际运行的扩展算子的映射。

Caffe网络中具有相同类型名但计算功能不同的层，比如DetectionOutput层，需要使用算子映射指明为FSRDetectionOutput、SSDDetectionOutput等检测算子类型，否则生成离线模型会执行失败。

该参数仅适用与Caffe框架。

## 关联参数

[--framework](--framework.md)取值为0时，才支持使用该参数；当[--framework](--framework.md)取值为1且为MindSpore框架网络模型时，设置本参数无效，但模型转换成功。

## 参数取值

**参数值：**
扩展算子映射配置文件路径和文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

## 推荐配置及收益

无。

## 示例

扩展算子映射配置文件内容示例如下（文件名举例为：_opname\_map.cfg_）：

```text
DetectionOutput:SSDDetectionOutput
```

将配置好的_opname\_map.cfg_上传到ATC工具所在服务器任意目录，例如上传到_$HOME/module_，使用示例如下：

```bash
atc --op_name_map=$HOME/module/opname_map.cfg ...
```

## 依赖约束

无。
