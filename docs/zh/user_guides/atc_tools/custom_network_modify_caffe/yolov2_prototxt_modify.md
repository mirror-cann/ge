# YOLOv2网络模型prototxt修改

> [!NOTE]说明
>
>本章节所有的代码样例都不能直接复制到网络模型中使用，需要用户根据使用的网络模型，自行调整相应参数，比如bottom、top中的参数要和具体网络模型中的bottom、top一一对应，并且bottom和top对应的参数顺序不能更改。

1. 修改Region算子。

    由于Yolo算子+DetectionOutput算子才是特征检测网络完整的后处理逻辑，在增加YoloV2DetectionOutput算子之前需要将Region过程算子，替换为Yolo算子。

    参见《AI框架算子支持清单》\>支持Caffe算子清单，Yolo算子有一个输入，三个输出；基于该原则，修改后的Yolo算子如下，对比情况如[图1](#fig1)所示，其中左边为原始算子prototxt，右边是适配AI处理器的prototxt。

    **图 1**  原始算子与修改后的算子比对2<a id="fig1"></a>
    ![图示](../figures/original_vs_modified_op_2.png "原始算子与修改后的算子比对2")

    构造的代码样例如下：

    ```textproto
    layer {
     bottom: "layer31-conv"
     top: "yolo_coords"
     top: "yolo_obj"
     top: "yolo_classes"
     name: "yolo"
     type: "Yolo"
     yolo_param {
      boxes: 5
      coords: 4
      classes: 80
      yolo_version: "V2"
      softmax: true
      background: false
        }
    }
    ```

    参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。

2. 最后一层增加YoloV2DetectionOutput算子。

    对于YoloV2网络，参考[扩展算子列表](extended_operator_list.md)在原始prototxt文件的最后增加后处理算子层YoloV2DetectionOutput ，参见《AI框架算子支持清单》\>支持Caffe算子清单，YoloV2DetectionOutput算子有四个输入，两个输出；根据上述原则，构造的后处理算子代码样例如下：

    ```textproto
    layer {
    name: "detection_out2"
    type: "YoloV2DetectionOutput"
    bottom: "yolo_coords"
    bottom: "yolo_obj"
    bottom: "yolo_classes"
    bottom: "img_info"
    top: "box_out"
    top: "box_out_num"
    yolov2_detection_output_param {
      boxes: 5
      classes: 80
      relative: true
      obj_threshold: 0.5
      score_threshold: 0.5
      iou_threshold: 0.45
      pre_nms_topn: 512
      post_nms_topn: 1024
      biases: 0.572730
      biases: 0.677385
      biases: 1.874460
      biases: 2.062530
      biases: 3.338430
      biases: 5.474340
      biases: 7.882820
      biases: 3.527780
      biases: 9.770520
      biases: 9.168280
      }
    }
    ```

    参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。

3. 新增输入：

    由于YoloV2DetectionOutput算子有img\_info输入，故模型输入时增加该输入。对比情况如[图2](#fig2)所示，其中左边为原始算子prototxt，右边是适配AI处理器的prototxt。

    **图 2**  原始算子与修改后的算子比对1<a id="fig2"></a>
    ![图示](../figures/original_vs_modified_op_1_2.png "原始算子与修改后的算子比对1-2")

    代码样例如下，参数为\[batch,4\]，4表示netH、netW、scaleH、scaleW四个维度。其中netH、netW为网络模型输入的HW，scaleH、scaleW为原始图片的HW。

    ```textproto
    input: "img_info"
    input_shape {
      dim: 1
      dim: 4
    }
    ```
