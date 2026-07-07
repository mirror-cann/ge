# --dynamic\_dims

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
全量芯片支持
<!-- end id2 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--dynamic_dims_res.md#id1 -->

<!-- npu="IPV350" id1 -->
IPV350：不支持
<!-- end id1 -->

## 功能说明

设置ND格式下动态维度的档位。适用于执行推理时，每次处理任意维度的场景。

为支持Transformer等网络在输入格式的维度不确定的场景，通过该参数实现ND格式下任意维度的档位设置。ND表示支持任意格式。

## 关联参数

该参数需要与[--input\_shape](--input_shape.md)、[--input\_format](--input_format.md)配合使用，不能与[--dynamic\_batch\_size](--dynamic_batch_size.md)、[--dynamic\_image\_size](--dynamic_image_size.md)、[--insert\_op\_conf](--insert_op_conf.md)同时使用。

## 参数取值

**参数值：**
通过"dim1,dim2,dim3;dim4,dim5,dim6;dim7,dim8,dim9"的形式设置。

**参数值格式：**
所有档位必须放在双引号中，档位之间使用英文**分号**分隔，每档内参数使用英文**逗号**分隔；每档内的dim值与[--input\_shape](--input_shape.md)参数中的-1标识的参数依次对应，[--input\_shape](--input_shape.md)参数中有几个-1，则每档必须设置几个维度。

**参数值约束：**

<!-- npu="A3,910b,910,310p,310b" id3 -->
- 针对如下产品，档位数约束为：档位数取值范围为\(1,100\]，即必须设置至少2个档位，最多支持100档配置，建议配置为3\~4档。

    <!-- npu="A3" id5 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id5 -->
    <!-- npu="910b" id6 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id6 -->
    <!-- npu="310b" id7 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id7 -->
    <!-- npu="310p" id8 -->
    Atlas 推理系列产品
    <!-- end id8 -->
    <!-- npu="910" id9 -->
    Atlas 训练系列产品
    <!-- end id9 -->
<!-- end id3 -->

<!-- npu="950" id4 -->
- 针对Ascend 950PR/Ascend 950DT，档位数约束为：档位数取值范围为\(1, 256\]，即必须设置至少2个档位，最多支持256档配置，建议配置为3\~4档。
<!-- end id4 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--dynamic_dims_res.md#id2 -->

## 推荐配置及收益

无。

## 示例

- 若网络模型只有一个输入：

    每档中的dim值与[--input\_shape](--input_shape.md)参数中的-1标识的参数依次对应，[--input\_shape](--input_shape.md)参数中有几个-1，则每档必须设置几个维度。例如：

    ATC参数取值为：

    ```bash
    atc --input_shape="data:1,-1"  --dynamic_dims="4;8;16;64" --input_format=ND ...
    ```

    则ATC在编译模型时，支持的data算子的shape为1,4; 1,8; 1,16; 1,64。

    ATC参数取值为：

    ```bash
    atc --input_shape="data:1,-1,-1"  --dynamic_dims="1,2;3,4;5,6;7,8" --input_format=ND
    ```

    则ATC在编译模型时，支持的data算子的shape为1,1,2; 1,3,4; 1,5,6; 1,7,8。

- 若网络模型有多个输入：

    每档中的dim值与网络模型输入参数中的-1标识的参数依次对应，网络模型输入参数中有几个-1，则每档必须设置几个维度。例如网络模型有三个输入，分别为data\(1,1,40,T\)，label\(1,T\)，mask\(T,T\) ，其中T为动态可变。则配置示例为：

    ```bash
    atc --input_shape="data:1,1,40,-1;label:1,-1;mask:-1,-1"  --dynamic_dims="20,20,1,1;40,40,2,2;80,60,4,4" --input_format=ND
    ```

    在ATC编译模型时，支持的输入dims组合档数分别为：

    第0档：data\(1,1,40,20\)+label\(1,20\)+mask\(1,1\)

    第1档：data\(1,1,40,40\)+label\(1,40\)+mask\(2,2\)

    第2档：data\(1,1,40,80\)+label\(1,60\)+mask\(4,4\)

## 依赖约束

- **使用约束：**

    不支持含有过程动态shape算子（网络中间层shape不固定）的网络。

- **接口约束：**

    如果模型转换时通过该参数设置了动态维度，则使用应用工程进行模型推理时，在**模型执行**接口之前：

  - 使用[aclmdlSetInputDynamicDims](../../../api/graph_engine_api/c/acl/aclmdlSetInputDynamicDims.md)接口，用于设置真实的维度。
  - 不使用[aclmdlSetInputDynamicDims](../../../api/graph_engine_api/c/acl/aclmdlSetInputDynamicDims.md)接口，则模型执行时，默认按照动态维度设置范围的最大值进行赋值。
