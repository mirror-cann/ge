# BatchedMatMul算子prototxt修改

> [!NOTE]说明
>
>本章节所有的代码样例都不能直接复制到网络模型中使用，需要用户根据使用的网络模型，自行调整相应参数，比如bottom、top中的参数要和具体网络模型中的bottom、top一一对应，并且bottom和top对应的参数顺序不能更改。

该算子用于输出两个张量的乘积：y=x1\*x2（x1和x2的张量相乘，x1和x2维度必须大于2而小于等于8）。如果网络模型使用该算子，则请参见该章节，修改相应prototxt后，再进行模型转换。

参见caffe.proto文件（该文件路径为$\{INSTALL\_DIR\}/include/proto），先在LayerParameter  message中添加自定义层参数的声明（如下自定义层在caffe.proto中已经声明，用户无需再次添加）：

```c++
message LayerParameter {
...
  optional BatchedMatMulParameter batched_matmul_param = 235;
...
}
```

参见caffe.proto文件，此算子的Type及属性定义如下：

```c++
message BatchedMatMulParameter{
    optional bool adj_x1 = 1 [default = false];
    optional bool adj_x2 = 2 [default = false];
}
```

参见《AI框架算子支持清单》\>支持Caffe算子清单，BatchedMatMul算子有2个输入，1个输出，基于上述原则，构造的代码样例如下：

```textproto
layer {
  name: "batchmatmul"
  type: "BatchedMatMul"
  bottom: "matmul_data_1"
  bottom: "matmul_data_2"
  top: "batchmatmul_1"
  batched_matmul_param {
  adj_x1:false
  adj_x2:true
  }
}
```

参数解释请参见《AI框架算子支持清单》\>支持Caffe算子清单。
