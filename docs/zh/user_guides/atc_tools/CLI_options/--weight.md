# --weight

## 产品支持情况

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--weight_res.md#id1 -->

<!-- npu="IPV350" id1 -->
- IPV350：不支持
<!-- end id1 -->

<!-- npu="950" id2 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id2 -->

<!-- npu="A3" id3 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id3 -->

<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id4 -->

<!-- npu="310b" id5 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id5 -->

<!-- npu="310p" id6 -->
- Atlas 推理系列产品：支持
<!-- end id6 -->

<!-- npu="910" id7 -->
- Atlas 训练系列产品：支持
<!-- end id7 -->

## 功能说明

原始网络模型权重文件路径与文件名，当原始网络模型是Caffe时需要指定。

## 关联参数

当原始模型为Caffe框架时，需要和[--model](--model.md)参数配合使用。

## 参数取值

**参数值：**
权重文件路径与文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

## 推荐配置及收益

无。

## 示例

```bash
atc --mode=0 --model=$HOME/module/resnet50.prototxt --weight=$HOME/module/resnet50.caffemodel --framework=0 --soc_version=<soc_version>  --output=$HOME/module/out/caffe_resnet50 ...
```

## 依赖约束

无。
