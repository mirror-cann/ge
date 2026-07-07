# SENet网络模型prototxt修改

> [!NOTE]说明
>
>本章节所有的代码样例都不能直接复制到网络模型中使用，需要用户根据使用的网络模型，自行调整相应参数，比如bottom、top中的参数要和具体网络模型中的bottom、top一一对应，并且bottom和top对应的参数顺序不能更改。

该网络模型中的Axpy算子，需要修改为Reshape、Scale、Eltwise三个算子，修改样例如下：

![图示](../figures/extended_op_senet.png)

修改后的代码样例如下：

```textproto
layer {
  name: "conv3_1_axpy_reshape"
  type: "Reshape"
  bottom: "conv3_1_1x1_up"
  top: "conv3_1_axpy_reshape"
  reshape_param {
    shape {
      dim: 0
      dim: -1
    }
  }
}
layer {
  name: "conv3_1_axpy_scale"
  type: "Scale"
  bottom: "conv3_1_1x1_increase"
  bottom: "conv3_1_axpy_reshape"
  top: "conv3_1_axpy_scale"
  scale_param {
    axis: 0
    bias_term: false
  }
}
layer {
  name: "conv3_1_axpy_eltwise"
  type: "Eltwise"
  bottom: "conv3_1_axpy_scale"
  bottom: "conv3_1_1x1_proj"
  top: "conv3_1"
}
```

Reshape、Scale、Eltwise算子相关参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。
