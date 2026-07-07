# Parser解析接口支持的配置参数

**产品支持情况：如下参数为全量芯片支持。**

## INPUT\_FP16\_NODES

指定输入数据类型为FP16的输入节点名称。

例如："node\_name1;node\_name2"，指定的节点必须放在双引号中，节点中间使用英文分号分隔。

**配置示例：**

```c++
{ge::AscendString(ge::ir_option::INPUT_FP16_NODES), ge::AscendString("input1;input2")},
```

## IS\_INPUT\_ADJUST\_HW\_LAYOUT

用于指定网络输入数据类型是否为FP16，数据格式是否为NC1HWC0。

该参数需要与INPUT\_FP16\_NODES配合使用。若IS\_INPUT\_ADJUST\_HW\_LAYOUT参数设置为true，对应INPUT\_FP16\_NODES节点的输入数据类型为FP16，输入数据格式为NC1HWC0。

**参数取值**：取值范围为false或true，默认值为false。

**配置示例：**

```c++
{ge::AscendString(ge::ir_option::INPUT_FP16_NODES), ge::AscendString("input1;input2")},
{ge::AscendString(ge::ir_option::IS_INPUT_ADJUST_HW_LAYOUT), ge::AscendString("true,true")},
```

## OUTPUT

指定转图后计算图名称。

**配置示例：**

```c++
{ge::AscendString(ge::ir_option::OUTPUT), ge::AscendString("newIssue")},
```

## IS\_OUTPUT\_ADJUST\_HW\_LAYOUT

用于指定网络输出的数据类型是否为FP16，数据格式是否为NC1HWC0。

该参数需要与OUT\_NODES配合使用。

若IS\_OUTPUT\_ADJUST\_HW\_LAYOUT参数设置为true，对应OUT\_NODES中输出节点的输出数据类型为FP16，数据格式为NC1HWC0。

**参数取值**：false或true，默认为false。

**配置示例：**

```c++
{ge::AscendString(ge::ir_option::OUT_NODES), ge::AscendString("add_input:0")},
{ge::AscendString(ge::ir_option::IS_OUTPUT_ADJUST_HW_LAYOUT), ge::AscendString("true")},
```

## OUT\_NODES

指定某层输出节点（算子）作为网络模型的输出或指定网络模型输出的名称。

如果不指定输出节点（算子名称），则模型的输出默认为最后一层的算子信息，如果指定，则以指定的为准。

某些情况下，用户想要查看某层算子参数是否合适，则需要将该层算子的参数输出，即可以在模型编译时通过该参数指定输出某层算子，模型编译后，在相应.om模型文件最后即可以看到指定输出算子的参数信息，如果通过.om模型文件无法查看，则可以将.om模型文件转换成JSON格式后查看。

支持三种格式：

- **格式1**："node\_name1:0;node\_name1:1;node\_name2:0"。

    网络模型中的节点（node\_name）名称，指定的输出节点必须放在双引号中，节点中间使用英文分号分隔。node\_name必须是模型编译前的网络模型中的节点名称，冒号后的数字表示第几个输出，例如node\_name1:0，表示节点名称为node\_name1的第1个输出。

    **配置示例：**

    ```c++
    {ge::AscendString(ge::ir_option::OUT_NODES), ge::AscendString("add_input:0")},
    ```

- **格式2**："topname1;topname2"。\(仅支持**CAFFE**\)

    某层输出节点的topname，指定的输出节点必须放在双引号中，节点中间使用英文分号分隔。topname必须是模型编译前的caffe网络模型中的layer的某个top名称；若几个layer有相同的topname，最后一个layer为准。

    **配置示例：**

    ```c++
    {ge::AscendString(ge::ir_option::OUT_NODES), ge::AscendString("res5c_branch2c")},
    ```

- **格式3**："output1;output2;output3"（仅支持**ONNX**）

    指定网络模型输出的名称（output的name），指定的output的name必须放在双引号中，多个output的name中间使用英文分号分隔。output必须是网络模型的输出。

    **配置示例：**

    ```c++
    {ge::AscendString(ge::ir_option::OUT_NODES), ge::AscendString("output1")},
    ```

## ENABLE\_SCOPE\_FUSION\_PASSES

指定编译时需要生效的融合规则列表。

融合规则分类如下：

- 内置融合规则：

  - General：各网络通用的scope融合规则；默认生效，不支持用户指定失效。

  - Non-General：特定网络适用的scope融合规则；默认不生效，用户可以通过ENABLE\_SCOPE\_FUSION\_PASSES参数指定生效的融合规则列表。

- 用户自定义的融合规则：

  - General：加载后默认生效，暂不提供用户指定失效的功能。

  - Non-General：加载后默认不生效，用户可以通过ENABLE\_SCOPE\_FUSION\_PASSES参数指定生效的融合规则列表。

指定的融合规则必须放在双引号中，规则中间使用英文逗号分隔。

**配置示例**：

```c++
{ge::AscendString(ge::ir_option::ENABLE_SCOPE_FUSION_PASSES), ge::AscendString("ScopePass1,ScopePass2")},
```

> [!NOTE]说明
>仅aclgrphParseTensorFlow支持该参数。

## INPUT\_DATA\_NAMES

指定模型文件输入节点的name和index属性的映射关系。系统按照输入name的顺序，设置对应输入节点的index属性。

**配置示例**：

```c++
{ge::AscendString(ge::ir_option::INPUT_DATA_NAMES), ge::AscendString("Placeholder,Placeholder_1")},
```

## INPUT\_SHAPE

模型输入的shape信息。

**参数取值**：

- 静态shape。
  - 若模型为单个输入，则shape信息为"input\_name:n,c,h,w"；指定的节点必须放在双引号中。
  - 若模型有多个输入，则shape信息为"input\_name1:n1,c1,h1,w1;input\_name2:n2,c2,h2,w2"；不同输入之间使用英文分号分隔，input\_name必须是转换前的网络模型中的节点名称。

- 若原始模型中输入数据的某个或某些维度值不固定，当前支持通过设置shape范围的方式转换模型。
  - 设置shape范围。

        Atlas 200I/500 A2 推理产品**不支持设置shape范围**。

        IPV350**不支持设置shape范围**。

        设置INPUT\_SHAPE参数时，可将对应维度的值设置为范围。

    - 支持按照name设置："input\_name1:n1,c1,h1,w1;input\_name2:n2,c2,h2,w2"，例如："input\_name1:8\~20,3,5,-1;input\_name2:5,3\~9,10,-1"。指定的节点必须放在双引号中，节点中间使用英文分号分隔。input\_name必须是转换前的网络模型中的节点名称。如果用户知道data节点的name，推荐按照name设置。
    - 支持按照index设置："n1,c1,h1,w1;n2,c2,h2,w2"，例如："8\~20,3,5,-1;5,3\~9,10,-1"。可以不指定节点名，节点按照索引顺序排列，节点中间使用英文分号分隔。按照index设置shape范围时，data节点需要设置属性index，说明是第几个输入，index从0开始。

        如果用户不想指定维度的范围或具体取值，则可以将其设置为-1，表示此维度可以使用\>=0的任意取值，该场景下取值上限为int64数据类型表达范围，但受限于host和device侧物理内存的大小，用户可以通过增大内存来支持。

- shape为标量。
  - 非动态分档场景：

        shape为标量的输入，可选配置，例如模型有两个输入，input\_name1为标量，即shape为"\[\]"形式，input\_name2输入shape为\[n2,c2,h2,w2\]，则shape信息为"**input\_name1:**;input\_name2:n2,c2,h2,w2"，指定的节点必须放在双引号中，不同输入之间使用英文分号分隔，input\_name必须是转换前的网络模型中的节点名称；标量的输入如果配置，则配置为空。

**配置示例：**

- 静态shape，例如某网络的输入shape信息，输入1**：**input\_0\_0 \[16,32,208,208\]，输入2：input\_1\_0 \[16,64,208,208\]，则INPUT\_SHAPE的配置信息为：

    ```c++
    {ge::AscendString(ge::ir_option::INPUT_SHAPE), ge::AscendString("input_0_0:16,32,208,208;input_1_0:16,64,208,208")},
    ```

- 设置shape范围的示例：

    ```c++
    {ge::AscendString(ge::ir_option::INPUT_SHAPE), ge::AscendString("input_0_0:1~10,32,208,208;input_1_0:16,64,100~208,100~208")},
    ```

- shape为标量
  - 非动态分档场景

    shape为标量的输入，可选配置。例如模型有两个输入，**input\_name1**为标量，input\_name2输入shape为\[16,32,208,208\]，配置示例为：

    ```c++
    {ge::AscendString(ge::ir_option::INPUT_SHAPE), ge::AscendString("input_name1:;input_name2:16,32,208,208")},
    ```

    上述示例中的**input\_name1**为可选配置**。**

> [!NOTE]说明
>
>INPUT\_SHAPE为可选设置。如果不设置，系统直接读取对应Data节点的shape信息，如果设置，以此处设置的为准，同时刷新对应Data节点的shape信息。
