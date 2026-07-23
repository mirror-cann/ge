# --insert\_op\_conf

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
全量芯片支持
<!-- end id1 -->

<!-- npu="IPV350" id7 -->
IPV350：不支持
<!-- end id7 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--insert_op_conf_res.md#id1 -->

## 功能说明

插入算子的配置文件路径与文件名，例如AIPP预处理算子。

若使用该参数后，输入数据类型为UINT8。

## 关联参数

- 若配置了该参数，则不能对同一个输入节点同时使用[--input\_fp16\_nodes](--input_fp16_nodes.md)参数。
- 该参数不能与[--dynamic\_dims](--dynamic_dims.md)同时使用。

## 参数取值

**参数值：**
插入算子的配置文件路径与文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束：**
文件后缀不局限于.cfg格式，但是配置文件中的内容需要满足prototxt格式。

## 推荐配置及收益

无。

## 示例

下面以插入AIPP预处理算子为例进行说明，配置文件内容示例如下（文件名为举例为：_insert\_op.cfg_），关于AIPP预处理配置文件的详细配置说明，请查看[开启AIPP](../AIPP/enable_aipp.md)章节。

```textproto
aipp_op {
    aipp_mode:static
    input_format:YUV420SP_U8
    csc_switch:true
    var_reci_chn_0:0.00392157
    var_reci_chn_1:0.00392157
    var_reci_chn_2:0.00392157
}
```

将配置好的_insert\_op.cfg_文件上传到ATC工具所在服务器任意目录，例如上传到$HOME/module，使用示例如下：

```bash
atc --insert_op_conf=$HOME/module/insert_op.cfg ...
```

## 依赖约束

- 如果用户设置了**静态AIPP**功能，同时又通过[--input\_shape](--input_shape.md)设置了动态shape范围参数，则：

    如果模型只有一个输入，该场景不支持；如果模型有多个输入，则必须对不同的输入节点进行设置，比如一个输入节点设置静态AIPP，另外一个节点设置动态shape。

- 如果用户设置了**静态AIPP**功能，同时又通过[--dynamic\_image\_size](--dynamic_image_size.md)设置了动态分辨率（输入图片的宽和高不确定）：

    该场景下，AIPP配置文件中不能开启Crop和Padding功能，并且需要将配置文件中的src\_image\_size\_w和src\_image\_size\_h取值设置为0。

- 如果用户设置了**动态AIPP**功能，同时又通过[--input\_shape](--input_shape.md)设置了动态shape范围参数，则AIPP输出的宽和高要在[--input\_shape](--input_shape.md)所设置的范围内。
- 如果用户设置了**动态AIPP**功能，同时又通过[--dynamic\_batch\_size](--dynamic_batch_size.md)设置了动态batch\_size：

    实际推理时，调用[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)接口设置动态AIPP相关参数值时，需确保batch\_size要设置为最大Batch数。

- 如果用户设置了**动态AIPP**功能，同时又通过[--dynamic\_image\_size](--dynamic_image_size.md)设置了动态分辨率（输入图片的宽和高不确定）：

    实际推理时，调用[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)接口，设置动态AIPP相关参数值时，不能开启Crop和Padding功能。该场景下，还需要确保通过aclmdlSetInputAIPP接口设置的宽和高与[aclmdlSetDynamicHWSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicHWSize.md)接口设置的宽、高相等，都必须设置成动态分辨率最大档位的宽、高。
