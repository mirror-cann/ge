# --input\_shape\_range

## 功能说明

**该参数已废弃，请勿使用。若涉及指定模型输入数据的shape范围，请使用[--input\_shape](--input_shape.md)。**

指定模型输入数据的shape范围。

## 关联参数

该参数不能与[--dynamic\_batch\_size](--dynamic_batch_size.md)、[--dynamic\_image\_size](--dynamic_image_size.md)、[--dynamic\_dims](--dynamic_dims.md)同时使用。

## 参数取值

**参数值：**
模型输入数据的shape范围信息，例如："input\_name1:\[n1\~n2,c1,h1,w1\];input\_name2:\[n2,c2,h2,w2\]"。指定的节点必须放在双引号中，节点中间使用英文分号分隔。input\_name必须是转换前的网络模型中的节点名称。

**参数值约束：**

- shape范围信息必须放在英文\[\]中。
- 该参数不限定维度，维度中的任一值都可以由用户指定取值范围。
- 如果用户不想指定维度的取值，则可以将其设置为-1，表示此维度可以使用\>=0的任意取值。

## 推荐配置及收益

无。

## 示例

```bash
atc --input_shape_range="input1:[8~20,3,5,-1];input2:[5,3~9,10,-1]" ...
```

## 依赖约束

- **使用约束：**
  - 该参数只适用于TensorFlow和ONNX网络模型。
  - 若使用该参数时，同时通过[--insert\_op\_conf](--insert_op_conf.md)设置了AIPP功能，则AIPP输出图片的宽和高要在[--input\_shape\_range](--input_shape_range.md)所设置的范围内。

- **接口约束：**

    如果模型转换时通过该参数设置了shape的范围，则使用应用工程进行模型推理时，需要在[aclmdlExecute](../../../api/graph_engine_api/c/acl/aclmdlExecute.md)接口之前，调用[aclmdlSetDatasetTensorDesc](../../../api/graph_engine_api/c/acl/aclmdlSetDatasetTensorDesc.md)接口，用于设置真实的输入Tensor描述信息（输入shape范围）；模型执行之后，调用[aclmdlGetDatasetTensorDesc](../../../api/graph_engine_api/c/acl/aclmdlGetDatasetTensorDesc.md)接口获取模型动态输出的Tensor描述信息；再进一步调用aclTensorDesc下的操作接口获取输出Tensor数据占用的内存大小、Tensor的Format信息、Tensor的维度信息等。
