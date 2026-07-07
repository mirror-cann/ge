# --input\_shape

## 产品支持情况

全量芯片支持

## 功能说明

指定模型输入数据的shape。

## 关联参数

动态分档场景，需要配合使用[--dynamic\_batch\_size](--dynamic_batch_size.md)（设置batch\_size档位）或[--dynamic\_image\_size](--dynamic_image_size.md)（设置分辨率档位）或[--dynamic\_dims](--dynamic_dims.md)（设置指定维度档位）参数。

## 参数取值

**参数值：**

- 原始模型为固定shape，--input\_shape参数为**可选**配置
  - 若模型为单个输入，则shape样例为`input_name:n,c,h,w`。
  - 若模型有多个输入，shape之间使用英文分号分隔，样例为`input_name1:n1,c1,h1,w1;input_name2:n2,c2,h2,w2`。

- 原始模型为非固定shape，--input\_shape参数**必须**配置

    若原始模型中输入数据的某个或某些维度值不固定，当前支持通过设置动态分档或设置shape范围两种方式转换模型：

  - 设置动态分档（静态shape），包括设置batch\_size档位、设置分辨率档位、设置指定维度档位。

    设置--input\_shape参数时，将对应维度值设置为-1，同时配合使用[--dynamic\_batch\_size](--dynamic_batch_size.md)（设置batch\_size档位）或[--dynamic\_image\_size](--dynamic_image_size.md)（设置分辨率档位）或[--dynamic\_dims](--dynamic_dims.md)（设置指定维度档位）参数。

  - 设置shape范围（动态shape）。

    <!-- npu="310b" id1 -->
    **Atlas 200I/500 A2 推理产品不支持设置shape范围。**
    <!-- end id1 -->

    <!-- npu="IPV350" id2 -->
    **IPV350不支持设置shape范围。**
    <!-- end id2 -->

    设置--input\_shape参数时，可将对应维度的值设置为范围，例如1\~10，**设置的range值范围必须有效**。

    如果用户不想指定维度的范围或具体取值，则可以将其设置为-1，模型执行时该维度被解析为\>=0的任意取值。

- 原始模型shape为标量
  - 非动态分档场景：

    shape为标量的输入，可选配置，例如模型有两个输入，input\_name1为标量，即shape为"\[\]"形式，input\_name2输入shape样例为\[n2,c2,h2,w2\]，则shape信息为`input_name1:;input_name2:n2,c2,h2,w2`；标量的输入如果配置，则配置为空。

  - 动态分档场景：

    如果模型输入中既有标量shape，又有支持动态分档的shape，则标量输入不能忽略，必须配置。例如模型有三个输入，分别为`A:[-1,c1,h1,w1]`、`B:[]`、`C:[n2,c2,h2,w2]`，则shape信息为`A:-1,c1,h1,w1;B:;C:n2,c2,h2,w2`，标量输入B必须配置。

**参数值约束：**

- 若模型有多个输入，则指定的节点必须放在双引号中，不同输入之间使用**英文分号**分隔，input\_name必须是转换前的网络模型中的节点名称。
- 若原始模型中输入数据的某个维度值不固定（例如input\_name1:？,h,w,c），通过Netron等可视化软件打开模型之后，输入信息样例如下：

    ![](../figures/input_info_example.png)

    该场景下--input\_shape参数必填，并可以进行如下操作：

  - 固定shape，将维度值设置为固定取值，例如，input\_name1:**1**,h,w,c，用于将输入数据某个维度不固定的原始模型转换为固定维度的离线模型。
  - 设置shape分档，例如设置为“-1”，与[--dynamic\_batch\_size](--dynamic_batch_size.md)参数配合使用。

- 设置shape范围时，若设置为-1，表示此维度可以使用\>=0的任意取值，该场景下取值上限为int64数据类型表示范围，但受限于host和device侧物理内存的大小，用户可以通过增大内存来支持。
- 若使用该参数时，同时通过[--insert\_op\_conf](--insert_op_conf.md)设置了AIPP功能，则AIPP输出图片的宽和高要在本参数所设置的范围内。

## 推荐配置及收益

无。

## 示例

- 固定shape，--input\_shape可选配置

    例如某网络的输入shape信息，输入1：`input_0_0 [16,32,208,208]`，输入2：`input_1_0 [16,64,208,208]`，则--input\_shape的配置信息为：

    ```bash
    atc --input_shape="input_0_0:16,32,208,208;input_1_0:16,64,208,208" ...
    ```

- 非固定shape，--input\_shape必须配置
  - 静态shape
    - 设置batch\_size档位的示例，请参见[--dynamic\_batch\_size](--dynamic_batch_size.md)。
    - 设置分辨率档位的示例，请参见[--dynamic\_image\_size](--dynamic_image_size.md)。
    - 设置指定维度档位的示例，请参见[--dynamic\_dims](--dynamic_dims.md)。

  - 动态shape

    设置shape范围的示例：

    ```bash
    atc --input_shape="input_0_0:1~10,32,208,208;input_1_0:16,64,100~208,100~208"  ...
    ```

- shape为标量
  - 非动态分档场景

    shape为标量的输入，可选配置。例如模型有两个输入，**input\_name1**为标量，input\_name2输入shape为\[16,32,208,208\]，配置示例为：

    ```bash
    atc --input_shape="input_name1:;input_name2:16,32,208,208" ...
    ```

    上述示例中的**input\_name1**为可选配置**。**

  - 动态分档场景

    shape为标量的输入，必须配置。例如模型有三个输入，shape信息分别为A:\[-1,32,208,208\]、**B:\[\]**、C:\[16,64,208,208\]，则配置示例为（A为动态分档输入，此处以设置batch\_size档位为例）：

    ```bash
    atc --input_shape="A:-1,32,208,208;B:;C:16,64,208,208"  --dynamic_batch_size="1,2,4" ...
    ```

## 依赖约束

- **使用约束：**
  - 如果用户通过[--input\_shape](--input_shape.md)设置了动态shape范围参数，同时又通过[--insert\_op\_conf](--insert_op_conf.md)参数配置了动态AIPP功能，则AIPP输出的宽和高要在[--input\_shape](--input_shape.md)所设置的范围内。
  - 如果用户通过[--input\_shape](--input_shape.md)设置了动态shape范围参数，同时又通过[--insert\_op\_conf](--insert_op_conf.md)参数配置了静态AIPP功能，则：

    如果模型只有一个输入，该场景不支持；如果模型有多个输入，则必须对不同的输入节点进行设置，比如一个输入节点设置静态AIPP，另外一个节点设置动态shape。

- **接口约束：**

    如果模型转换时通过该参数设置了shape的范围，使用应用工程进行模型推理时，需在[aclmdlExecute](../../../api/graph_engine_api/c/acl/aclmdlExecute.md)接口之前，调用[aclmdlSetDatasetTensorDesc](../../../api/graph_engine_api/c/acl/aclmdlSetDatasetTensorDesc.md)接口，用于设置真实的输入Tensor描述信息（输入shape范围）；模型执行之后，调用[aclmdlGetDatasetTensorDesc](../../../api/graph_engine_api/c/acl/aclmdlGetDatasetTensorDesc.md)接口获取模型动态输出的Tensor描述信息；再进一步调用[aclTensorDesc](../../../api/graph_engine_api/c/acl/aclTensorDesc.md)下的操作接口获取输出Tensor数据占用的内存大小、Tensor的Format信息、Tensor的维度信息等。
