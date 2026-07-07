# SSD网络模型prototxt修改

> [!NOTE]说明
>
>本章节所有的代码样例都不能直接复制到网络模型中使用，需要用户根据使用的网络模型，自行调整相应参数，比如bottom、top中的参数要和具体网络模型中的bottom、top一一对应，并且bottom和top对应的参数顺序不能更改。

对于SSD网络，参考[扩展算子列表](extended_operator_list.md)在原始prototxt文件的最后增加后处理算子层SSDDetectionOutput。

参见caffe.proto文件（该文件路径为$\{INSTALL\_DIR\}/include/proto），先在LayerParameter  message中添加自定义层参数的声明（如下自定义层在caffe.proto中已经声明，用户无需再次添加）：

```c++
message LayerParameter {
...
  optional SSDDetectionOutputParameter ssddetectionoutput_param = 232;
...
}
```

参见caffe.proto文件，此算子的Type及属性定义如下：

```c++
message SSDDetectionOutputParameter {
    optional int32 num_classes= 1 [default = 2];
    optional bool share_location = 2 [default = true];
    optional int32 background_label_id = 3 [default = 0];
    optional float iou_threshold = 4 [default = 0.45];
    optional int32 top_k = 5 [default = 400];
    optional float eta = 6 [default = 1.0];
    optional bool variance_encoded_in_target = 7 [default = false];
    optional int32 code_type = 8 [default = 2];
    optional int32 keep_top_k = 9 [default = 200];
    optional float confidence_threshold = 10 [default = 0.01];
}
```

参见《AI框架算子支持清单》\>支持Caffe算子清单，SSDDetectionOutput算子有3个输入，2个输出，基于上述原则，构造的代码样例如下：

```textproto
layer {
  name: "detection_out"
  type: "SSDDetectionOutput"
  bottom: "bbox_delta"
  bottom: "score"
  bottom: "anchors"
  top: "out_boxnum"
  top: "y"
  ssddetectionoutput_param {
    num_classes: 2
    share_location: true
    background_label_id: 0
    iou_threshold: 0.45
    top_k: 400
    eta: 1.0
    variance_encoded_in_target: false
    code_type: 2
    keep_top_k: 200
    confidence_threshold: 0.01
  }
}
```

- bottom输入中的bbox\_delta对应caffe原始网络中的mbox\_loc，score对应caffe原始网络中的mbox\_conf\_flatten，anchors对应caffe原始网络中的mbox\_priorbox；num\_classes取值需要与原始网络模型中的取值保持一致。
- top输出多batch场景下:
  - out\_boxnum输出shape是\(batchnum,8\)，每个batchnum的第一个值是实际框的个数。
  - y输出shape是\(batchnum,len,8\)，其中len是keep\_top\_k 128对齐后的取值（如batch为2，keep\_top\_k为200，则最后输出shape为\(2,256,8\)），前256\*8个数据为第一个batch的结果。

参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。
