# 什么是单算子描述文件

<!-- npu="IPV350" id1 -->
**IPV350不支持单算子特性。**
<!-- end id1 -->

单算子描述文件是基于Ascend IR定义的单个算子的定义文件，包括算子的输入、输出及属性等信息，借助该文件转换成适配AI处理器的离线模型后，可以验证单算子的功能。

单算子描述文件是由OpDesc数组构成的JSON文件，参数构成以及解释如下：

**表 1**  OpDesc参数说明<a id="table1"></a>

| 属性名 | 类型 | 说明 | 是否必填 |
| --- | --- | --- | --- |
| compile_flag | INT32 | 该参数废弃，不建议使用，后续版本将会删除。<br>编译类型。取值如下：<br><br>  - 0：表示进行精确编译。精确编译是指按照用户指定的维度信息、在编译时系统内部不做任何转义直接编译，其中，AI CPU算子不受该标记影响。<br>  - 1：表示进行模糊编译。模糊编译是指对于支持动态Shape的算子，在编译时系统内部对可变维度做了泛化后再进行编译。如果用户无法获取算子的Shape范围，又想编译一次达到多次执行推理的目的时，可以使用模糊编译特性。<br><br>默认值为0。<br>使用约束：当前仅支持transformer网络模型涉及的算子。 | 否 |
| op | string | 算子类型。 | 是 |
| name | string | 单算子模型文件的名称。<br>如果不设置name参数，则模型文件名的命名规则默认为：序号_算子类型_输入的描述(dataType_format_shape)_输出的描述(dataType_format_shape)，例如，0_Add_3_2_3_3_3_2_3_3_3_2_3_3.om。<br>dataType以及format对应枚举值请从`${INSTALL_DIR}/include/graph/types.h`文件中查看，枚举值依次递增。其中，`${INSTALL_DIR}`请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：`/usr/local/Ascend/cann`。 | 否 |
| input_desc | TensorDesc数组 | 算子输入描述。 | 是 |
| output_desc | TensorDesc数组 | 算子输出描述。 | 是 |
| attr | Attr数组 | 算子属性。 | 否 |

**表 2**  TensorDesc数组参数说明

| 属性名 | 类型 | 说明 |
| --- | --- | --- |
| dynamic_input | string | 可选。动态输入，取值必须和算子信息库中该算子定义的输入name相同。<br>该参数用于设置算子动态输入的分组与动态输入的个数，例如算子原型定义中某算子的动态输入为：<br>.DYNAMIC_INPUT(x,...)<br>.DYNAMIC_INPUT(y,...)<br>则表示动态输入有两组，分别为x，y。每一组的输入个数，根据dynamic_input的个数确定。具体设置原则可以参见TensorDesc数组中name参数的说明。<br><br>  - 如果构造的单算子描述文件中已经设置过name参数，则该参数可选。<br>  - 如果构造的单算子描述文件中没有name参数，则该参数必填。<br>  - 如果同时存在dynamic_input和name参数，则以name参数设置的为准。 |
| format | string | 必填。Tensor计算过程中实际使用的格式，又称运行时格式，对应Device上计算使用的格式。<br>当前支持的Format格式以及对应的枚举如下：<br><br>  - NCHW: 0<br>  - NHWC: 1<br>  - ND: 2，表示支持任意格式。<br>  - NC1HWC0: 3，5维数据格式。<br>  - FRACTAL_Z: 4，用于定义卷积权重的数据格式。<br>  - FRACTAL_NZ: 29，分形格式。关于上述Format详细解释请参见[关键概念](../overview/atc_overview.md#关键概念)。<br>  - RESERVED: 40，当存在可选输入，且可选输入没有输入数据时，则必须将可选输入的Format配置为RESERVED，同时将type配置为UNDEFINED；若可选输入有输入数据时，则按其输入数据的format、type配置即可。<br><br>模型转换完毕，上述Format在对应om离线模型文件名中以对应的枚举呈现，例如若输入为NHWC格式，则展示为1。 |
| origin_format | string | 可选。Tensor输入时的原始格式，指未经任何转换的原始图像格式。<br>不带此字段时，默认Tensor计算过程中使用的Format与原始Format一致。 |
| name | string | 可选。Tensor的名称。算子的输入为动态输入时，需要设置该字段。<br>该参数用于设置每一组动态输入中，具体输入的名称，每一个输入名称为算子原型中定义的输入名称+编号，编号根据dynamic_input的个数确定，从0开始依次递增。<br><br>  - 如果构造的单算子描述文件中已经设置过dynamic_input参数，则该参数可选。<br>  - 如果构造的单算子描述文件中没有dynamic_input参数，则该参数必填。<br>  - 如果同时存在dynamic_input和name参数，则以name参数设置的为准。 |
| shape | int数组 | 必填。Tensor计算过程中实际使用的Shape，例如[1, 224, 224, 3]，实际Shape乘积不能超过int32最大值（2147483647）。<br><br>  - 静态Shape场景：Shape维度以及取值都为固定值，该场景下不需要再配置shape_range参数。<br>  - Shape为常量场景：如果希望指定算子输入、输出Shape为标量，则该参数需要设置为"[]"形式，比如"shape": []。该场景下不需要再配置shape_range参数。<br>  - 动态Shape场景，Shape取值有如下场景：<br>  &nbsp;&nbsp;· Shape维度确定，但是某一维度的取值不确定，则该不确定的维度取值设置为“-1”，例如[16,-1,20,-1]，该场景下还需要与shape_range参数配合使用，用于给出“-1”维度的取值范围。例如：<br> &nbsp;&nbsp;&nbsp;&nbsp;"shape": [-1,16],<br> &nbsp;&nbsp;&nbsp;&nbsp;"shape_range": [[0,32]],<br>  &nbsp;&nbsp;· Shape维度也不确定，该场景下Shape取值为“-2”，例如"shape": [-2]，该场景下不需要配置shape_range参数（当前版本暂不支持）。<br>动态Shape算子执行场景下，算子执行环境中的算子库包安装版本需与算子模型编译环境的版本一致，否则在加载算子时会报错。详情可参见[算子库包版本问题导致加载单算子失败](../FAQ/operator_library_version_error.md)。 |
| origin_shape | string | 可选。Tensor输入时的原始Shape。<br>不带此字段时，默认Tensor计算过程中使用的Shape与原始Shape一致。 |
| type | string | 必填。Tensor的数据类型，支持的type以及对应的枚举如下：<br><br>  - bool: 12<br>  - int8: 2<br>  - uint8: 4<br>  - int16: 6<br>  - uint16: 7<br>  - int32: 3<br>  - uint32: 8<br>  - int64: 9<br>  - uint64: 10<br>  - float16/fp16/half: 1<br>  - float/float32: 0<br>  - double: 11<br>  - complex32: 33<br>  - complex64: 16<br>  - complex128: 17<br>  - uint1: 30<br>  - bfloat16: 27<br>  - int4: 29<br>  - UNDEFINED: 28，当存在可选输入，且可选输入没有输入数据时，则必须将可选输入的type配置为UNDEFINED，同时将format配置为RESERVED；若可选输入有输入数据时，则按其输入数据的format、type配置即可。<br><br>模型转换完毕，上述type在对应om离线模型文件名中以对应的枚举呈现，例如若输入为int8类型，则展示为2。 |
| shape_range | int[2]数组 | 可选。Shape为动态时（不包括-2场景），unknown shape的取值范围。<br>例如，若Shape取值为[16,-1,20,-1]：其中的-1表示unknown shape。<br>shape_range取值为[1,128],[1,-1]：[1,128]表示从1到128的取值范围，对应Shape参数中第一个-1；[1,-1]表示从1到无穷大的取值范围，对应Shape参数中第二个-1。 |
| is_const | bool | 可选，表示输入是否为常量：<br><br>  - true：常量。<br>  - false：默认值，非常量。 |
| const_value | list | 可选，常量取值。<br>当前仅支持一维list配置，list中具体配置个数由Shape取值决定。例如，Shape取值为2，则const_value中列表个数为2。<br>取值类型由type决定，假设type取值为float16，则单算子编译时会自动将const_value中的取值转换为float16格式的取值。 |

**表 3**  Attr数组参数说明

| 属性名 | 类型 | 说明 |
| --- | --- | --- |
| name | string | 必填。属性名。 |
| type | string | 必填。属性值的类型，支持的类型有：<br><br>  - bool<br>  - int<br>  - float<br>  - string<br>  - list_bool<br>  - list_int<br>  - list_float<br>  - list_string<br>  - list_list_int<br>  - data_type |
| value | 由type的取值决定 | 必填。属性值，根据type不同，属性值不同，举例如下：<br><br>  - bool: true/false<br>  - int: 10<br>  - float: 1.0<br>  - string: "NCHW"<br>  - list_bool: [false, true]<br>  - list_int: [1, 224, 224, 3]<br>  - list_float: [1.0, 0.0]<br>  - list_string: ["str1","str2"]<br>  - list_list_int: [[1, 3, 5, 7], [2, 4, 6, 8]]<br>  - data_type: "DT_FLOAT"或该枚举值对应的数字，例如0。其他取值请参见`${INSTALL_DIR}/include/graph/types.h`中DataType的枚举值或枚举值对应的数字。其中，`${INSTALL_DIR}`请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：`/usr/local/Ascend/cann`。 |
