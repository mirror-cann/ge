# --input\_format

## 产品支持情况

全量芯片支持

## 功能说明

指定模型输入数据的格式。

## 关联参数

无。

## 参数取值

**参数值：**

- 当原始框架为Caffe时，支持NCHW、ND（表示支持任意维度格式，N<=4）两种格式，默认为NCHW。
- 当原始框架为ONNX时，支持NCHW、NCDHW、ND（表示支持任意维度格式，N<=4）三种格式，默认为NCHW。
- 当原始框架是TensorFlow时，支持NCHW、NHWC、ND、NCDHW、NDHWC五种输入格式，默认为NHWC。
  - 如果TensorFlow模型是通过ONNX模型转换工具输出的，则该参数必填，且值为NCHW。
  - 如果原始模型中含有带data\_format入参的算子，则该参数必填，推荐取值为ND，模型转换过程中会根据data\_format属性的算子，推导出具体的format。若用户无法确定输入数据格式，则推荐指定为ND。

- 当原始框架为MindSpore时，只支持配置为NCHW，设置为其它值无效，但模型转换成功。

**参数默认值：**
Caffe、MindSpore、ONNX默认为NCHW；TensorFlow默认为NHWC。

**参数值约束：**

- 如果模型转换时开启AIPP，在进行推理业务时，输入图片数据要求为NHWC排布，该场景下最终与AIPP连接的输入节点的格式被强制改成NHWC，可能与atc模型转换命令中[--input\_format](--input_format.md)参数指定的格式不一致。
- 如果同时配置了[--insert\_op\_conf](--insert_op_conf.md)参数，则[--input\_format](--input_format.md)参数只能配置为NCHW、NHWC。
- 若模型有多个输入，不同输入需要设置为相同的数据格式。

## 推荐配置及收益

无。

## 示例

```bash
atc --input_format=NCHW ...
```

## 依赖约束

无。
