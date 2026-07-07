# FasterRCNN网络模型prototxt修改

> [!NOTE]说明
>
>本章节所有的代码样例都不能直接复制到网络模型中使用，需要用户根据使用的网络模型，自行调整相应参数，比如bottom、top中的参数要和具体网络模型中的bottom、top一一对应，并且bottom和top对应的参数顺序不能更改。

如下以FasterRCNN Resnet34网络模型为例进行说明。

1. proposal算子修改。

    根据《AI框架算子支持清单》\>支持Caffe算子清单，该算子有3个输入，2个输出；根据上述原则，对原始proposal算子进行修改，type修改为caffe.proto文件中的类型，增加actual\_rois\_num输出节点。参见caffe.proto文件中的属性定义增加相应属性信息。修改情况如[图1](#fig1)所示，其中左边为原始算子prototxt，右边是适配AI处理器的prototxt。

    **图 1**  原始算子与修改后的算子比对1<a id="fig1"></a>
    ![图示](../figures/original_vs_modified_op_1.png "原始算子与修改后的算子比对1")

    代码样例如下：

    ```textproto
    layer {
    name: "faster_rcnn_proposal"
    type: "Proposal"                  // 算子Type


    bottom: "rpn_cls_prob_reshape"
    bottom: "rpn_bbox_pred"
    bottom: "im_info"
    top: "rois"
    top: "actual_rois_num"        // 增加的算子输出
      proposal_param {

      feat_stride: 16
      base_size: 16
      min_size: 16
      pre_nms_topn: 3000
      post_nms_topn: 304
      iou_threshold: 0.7
      output_actual_rois_num: true
      }
    }
    ```

    参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。

2. 最后一层增加FSRDetectionOutput算子，用于输出最终的检测结果。

    对于FasterRCNN网络，参考[扩展算子列表](extended_operator_list.md)，在原始prototxt文件的最后增加后处理算子层FSRDetectionOutput ，参见《AI框架算子支持清单》\>支持Caffe算子清单，FSRDetectionOutput算子有五个输入，两个输出，此算子的Type及属性定义如下：

    代码样例如下：

    ```textproto
    layer {
      name: "FSRDetectionOutput_1"
      type: "FSRDetectionOutput"

      bottom: "rois"
      bottom: "bbox_pred"
      bottom: "cls_prob"
      bottom: "im_info"
      bottom: "actual_rois_num"
      top: "actual_bbox_num1"
      top: "box1"

      fsrdetectionoutput_param {

        num_classes:3
        score_threshold:0.0
        iou_threshold:0.7
        batch_rois:1
      }
    }
    ```

    参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。
