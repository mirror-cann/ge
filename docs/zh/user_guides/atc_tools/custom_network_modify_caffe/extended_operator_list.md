# 扩展算子列表

## 过程算子

| 算子类型 | 说明 |
| --- | --- |
| [Proposal](extended_operator_rules.md#proposal) | 用于Faster R-CNN，根据rpn_cls_prob的foreground，rpn_bbox_pred中的bounding box regression修正anchors获得精确的proposals。 |
| [ROIPooling](extended_operator_rules.md#roipooling) | 用于Faster R-CNN中，对ROI（Region of interest）进行池化操作，主要用于目标检测任务。 |
| [PSROIPooling](extended_operator_rules.md#psroipooling) | 用于R-FCN中，进行位置敏感的候选区域池化操作，主要用于目标检测任务。 |
| [Reverse](extended_operator_rules.md#reverse) | 将输入tensor指定的轴进行反转。 |
| [Upsample](extended_operator_rules.md#upsample) | 使用pooling mask的上采样。<br>用于yolo网络。 |
| [Normalize](extended_operator_rules.md#normalize) | 将SSD网络中Channel维度中的元素在L2进行归一化。 |
| [Reorg](extended_operator_rules.md#reorg) | Reorg layer in Darknet，将通道数据转移到平面上，或反过来操作。<br>对应算子规格中的PassThrough算子。 |
| [ROIAlign](extended_operator_rules.md#roialign) | 从feature map中获取ROI（Region of interest）的特征矩阵。 |
| [ShuffleChannel](extended_operator_rules.md#shufflechannel) | 把channel维度分为[group, channel/Group]，再在channel维度中进行数据转置[channel/Group,group]。 |
| [Yolo](extended_operator_rules.md#yolo) | Yolo/Detection/Region算子，都需要替换为Yolo算子，对卷积网络输出的feature map生成检测框的坐标信息，置信度信息及类别概率。 |
| [PriorBox](extended_operator_rules.md#priorbox) | 用于SSD网络，根据输入参数，生成prior box。 |
| [SpatialTransformer](extended_operator_rules.md#spatialtransformer) | 用于仿射变换。 |

## 后处理算子

| 算子类型 | 说明 |
| --- | --- |
| YoloV3DetectionOutput | 此算子用于YOLOv3的后处理过程，对卷积网络输出的feature map生成检测框的坐标信息，置信度信息及类别概率。 |
| YoloV2DetectionOutput | 此算子用于YOLOv2的后处理过程，对卷积网络输出的feature map生成检测框的坐标信息，置信度信息及类别概率。 |
| SSDDetectionOutput | 此算子用于SSD网络的后处理过程，用于整合预选框、预选框偏移以及得分三项结果，最终输出满足条件的目标检测框、目标的label和得分。 |
| FSRDetectionOutput | 此算子用于Faster R-CNN网络的后处理过程，对结果进行分类，并对每个类输出最终的bbox数量、坐标、类别概率及类别索引。 |
