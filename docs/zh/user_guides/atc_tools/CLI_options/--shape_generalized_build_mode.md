# --shape\_generalized\_build\_mode

## 功能说明

**该参数后续版本会废弃，请勿使用。**

图编译时Shape的编译方式。

## 关联参数

该参数不能与[--input\_shape\_range](--input_shape_range.md)、[--dynamic\_batch\_size](--dynamic_batch_size.md)、[--dynamic\_image\_size](--dynamic_image_size.md)、[--dynamic\_dims](--dynamic_dims.md)同时使用。

## 参数取值

**参数值：**

- shape\_generalized：模糊编译，在编译时系统内部对可变维度做了泛化后再进行编译。如果算子Shape是固定，则可变维度会修改为-1（维度不变，例如原来Shape为4维，模糊编译后仍为4维）进行编译。

    该参数使用场景为：用户想编译一次达到多次执行推理的目的时，可以使用模糊编译特性。

- shape\_precise：（默认值）精确编译，是指按照用户指定的维度信息、在编译时系统内部不做任何转义直接编译。

**参数值约束：**
如果算子本身不支持动态Shape、只支持静态Shape（无可变维度），此时按照静态Shape编译算子，不按模糊编译做泛化。

[图1](#fig1)为编译的两种方式。

**图 1**  编译模式<a id="fig1"></a>
![图示](../figures/compilation_mode.png "编译模式")

## 推荐配置及收益

无。

## 示例

```bash
atc --shape_generalized_build_mode=shape_generalized ...
```

## 使用约束

如果模型转换时通过该参数设置了模糊编译，则使用应用工程进行模型推理时，需要在[aclmdlExecute](../../../api/graph_engine_api/c/acl/aclmdlExecute.md)接口之前，增加[aclmdlSetDatasetTensorDesc](../../../api/graph_engine_api/c/acl/aclmdlSetDatasetTensorDesc.md)接口，用于设置真实的shape取值。
