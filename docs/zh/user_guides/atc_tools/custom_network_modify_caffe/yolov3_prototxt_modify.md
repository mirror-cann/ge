# YOLOv3网络模型prototxt修改

> [!NOTE]说明
>
>本章节所有的代码样例都不能直接复制到网络模型中使用，需要用户根据使用的网络模型，自行调整相应参数，比如bottom、top中的参数要和具体网络模型中的bottom、top一一对应，并且bottom和top对应的参数顺序不能更改。

1. upsample算子upsample\_param属性参数修改。

    参见《AI框架算子支持清单》\>支持Caffe算子清单，需要将原始算子prototxt中的scale:2修改为scale:1  stride:2。

    对比情况如[图1](#fig1)所示，其中左边为原始算子prototxt，右边是适配AI处理器的prototxt。

    **图 1**  原始算子与修改后的算子比对4<a id="fig1"></a>
    ![](../figures/original_vs_modified_op_4.png "原始算子与修改后的算子比对4")

    参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。

2. 新增三个Yolo算子。

    由于Yolo算子+DetectionOutput算子才是特征检测网络完整的后处理逻辑，根据原始算子prototxt所示，在增加YoloV3DetectionOutput算子之前需要先增加三个Yolo算子。

    根据《AI框架算子支持清单》\>支持Caffe算子清单，该算子有1个输入，3个输出，根据上述原则，构造的Yolo算子代码样例如下：

    - 算子1代码样例：

        ```textproto
        layer {
         bottom: "layer82-conv"
         top: "yolo1_coords"
         top: "yolo1_obj"
         top: "yolo1_classes"
         name: "yolo1"
         type: "Yolo"
         yolo_param {
          boxes: 3
          coords: 4
          classes: 80
          yolo_version: "V3"
          softmax: true
          background: false
            }
        }
        ```

    - 算子2代码样例：

        ```textproto
        layer {
         bottom: "layer94-conv"
         top: "yolo2_coords"
         top: "yolo2_obj"
         top: "yolo2_classes"
         name: "yolo2"
         type: "Yolo"
         yolo_param {
          boxes: 3
          coords: 4
          classes: 80
          yolo_version: "V3"
          softmax: true
          background: false
         }
        }
        ```

    - 算子3代码样例：

        ```textproto
        layer {
         bottom: "layer106-conv"
         top: "yolo3_coords"
         top: "yolo3_obj"
         top: "yolo3_classes"
         name: "yolo3"
         type: "Yolo"
         yolo_param {
          boxes: 3
          coords: 4
          classes: 80
          yolo_version: "V3"
          softmax: true
          background: false
         }
        }
        ```

    参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。

3. 最后一层增加YoloV3DetectionOutput算子。

    对于YOLOv3网络，参考[扩展算子列表](extended_operator_list.md)在原始prototxt文件的最后增加后处理算子层YoloV3DetectionOutput ，参见《AI框架算子支持清单》\>支持Caffe算子清单，YoloV3DetectionOutput算子有十个输入，两个输出；根据该原则，构造的后处理算子代码样例如下：

    ```textproto
    layer {
           name: "detection_out3"
           type: "YoloV3DetectionOutput"
           bottom: "yolo1_coords"
           bottom: "yolo2_coords"
           bottom: "yolo3_coords"
           bottom: "yolo1_obj"
           bottom: "yolo2_obj"
           bottom: "yolo3_obj"
           bottom: "yolo1_classes"
           bottom: "yolo2_classes"
           bottom: "yolo3_classes"
           bottom: "img_info"
           top: "box_out"
           top: "box_out_num"
           yolov3_detection_output_param {
                               boxes: 3
                               classes: 80
                               relative: true
                               obj_threshold: 0.5
                               score_threshold: 0.5
                               iou_threshold: 0.45
                               pre_nms_topn: 512
                               post_nms_topn: 1024
                               biases_high: 10
                               biases_high: 13
                               biases_high: 16
                               biases_high: 30
                               biases_high: 33
                               biases_high: 23
                               biases_mid: 30
                               biases_mid: 61
                               biases_mid: 62
                               biases_mid: 45
                               biases_mid: 59
                               biases_mid: 119
                               biases_low: 116
                               biases_low: 90
                               biases_low: 156
                               biases_low: 198
                               biases_low: 373
                               biases_low: 326
           }
    }
    ```

    参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。

4. 新增输入：

    由于YoloV3DetectionOutput算子有img\_info输入，故模型输入时增加该输入。对比情况如[图2](#fig2)所示，其中左边为原始算子prototxt，右边是适配AI处理器的prototxt。

    **图 2**  原始算子与修改后的算子比对3<a id="fig2"></a>
    ![](../figures/original_vs_modified_op_3.png "原始算子与修改后的算子比对3")

    代码样例如下，参数为\[batch,4\]，4表示netH、netW、scaleH、scaleW四个维度。其中netH、netW为网络模型输入的HW，scaleH、scaleW为原始图片的HW。

    ```textproto
    input: "img_info"
    input_shape {
      dim: 1
      dim: 4
    }
    ```
