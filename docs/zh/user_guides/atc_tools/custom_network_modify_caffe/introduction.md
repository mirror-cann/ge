# 简介

本章节修改只适用于Caffe网络模型。

<!-- npu="950" id1 -->
Ascend 950PR/Ascend 950DT不支持Caffe框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id1 -->
<!-- npu="A3" id2 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品不支持Caffe框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id2 -->
<!-- npu="910b" id3 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品不支持Caffe框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id3 -->
<!-- npu="IPV350" id4 -->
IPV350：不支持Caffe框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id4 -->
<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/custom_network_modify_caffe/introduction_res.md#id1 -->

网络的算子可以分为如下几类：

- 标准算子：AI处理器支持的Caffe标准算子，比如Convolution等。
- 扩展算子：AI处理器支持的公开但非Caffe标准算子，分为两种：
  - 一种是基于Caffe框架进行自定义扩展的算子，比如Faster RCNN中的ROIPooling、SSD中的归一化算子Normalize等。
  - 另外一种是来源于其他深度学习框架的自定义算子，比如YOLOv2中Passthrough等。

Faster RCNN、SSD等网络模型都包含了一些原始Caffe框架中没有定义的算子结构，如ROIPooling、Normalize、PSROIPooling和Upsample等。为了使AI处理器能支持这些网络，需要对原始的Caffe框架网络模型进行扩展，降低开发者开发自定义算子/开发后处理代码的工作量。若开发者的Caffe框架网络模型中使用了这些扩展算子，在进行模型转换时，需要先在prototxt中修改/添加扩展层的定义，才能成功进行模型转换。

本章节提供了AI处理器的扩展算子列表，并给出了如何根据扩展算子修改prototxt文件的方法。
