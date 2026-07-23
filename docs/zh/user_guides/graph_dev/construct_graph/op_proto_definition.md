# 什么是算子原型

用户通过图引擎接口，逐个将算子添加到计算图中，构建成Ascend IR表示的计算图；在此之前，需要先了解算子原型的相关内容。

## 算子原型定义

算子原型主要描述了算子的输入输出、属性等信息以及算子在AI处理器上相关实现信息。

在算子开发阶段，通过使用REG\_OP宏，以“.”链接INPUT、OUTPUT、ATTR等接口注册算子的输入、输出和属性信息，最终以OP\_END\_FACTORY\_REG接口结束，完成算子的注册。注册代码实现如下所示：

```c++
namespace ge{
    REG_OP(OpType) // 算子类型名称
    .INPUT(x1, TensorType({DT_FLOAT, DT_INT32}))
    .INPUT(x2, TensorType({DT_FLOAT, DT_INT32}))
    .DYNAMIC_INPUT(a, TensorType({DT_FLOAT, DT_INT32}))
    .OPTIONAL_INPUT(b, TensorType({DT_FLOAT}))
    .OUTPUT(y, TensorType({DT_FLOAT, DT_INT32}))
    .DYNAMIC_OUTPUT(c, TensorType({DT_FLOAT, DT_INT32}))
    .ATTR(x, Type, DefaultValue)
    .REQUIRED_ATTR(d, Type)
    .OP_END_FACTORY_REG(OpType)
}
```

- REG\_OP\(_OpType_\)

    OpType：注册到AI处理器的自定义算子库的算子类型。

- INPUT\(_x1,_  TensorType\(\{_DT\_FLOAT,DT\_UINT8,..._  \}\)\)

    注册算子的输入信息。

  - x1：宏参数，算子的输入名称。
  - TensorType\(\{_DT\_FLOAT,DT\_UINT8,..._  \}\)：“\{ \}”中为此输入支持的数据类型的列表。

    若算子有多个输入，每个输入需要使用一条INPUT\(_x,_  TensorType\(\{_DT\_FLOAT,DT\_UINT8,..._  \}\)\)语句进行描述。

- DYNAMIC\_INPUT\(_a_, TensorType\(\{_DT\_FLOAT, DT\_INT32_,... \}\)\)

    算子为动态多输入场景下的输入信息注册。

  - a：宏参数，算子的输入名称，图运行时，会根据输入的个数自动生成a0、a1、a2……，序号依次递增。
  - TensorType\(\{_DT\_FLOAT, DT\_INT32_,... \}\)：“\{ \}”中为此输入支持的数据类型的列表。

- OPTIONAL\_INPUT\(_b_, TensorType\(\{_DT\_FLOAT,..._\}\)\)

    若算子输入为可选输入，可使用此接口进行算子输入的注册。

  - b：宏参数，算子输入的名称。
  - TensorType\(\{_DT\_FLOAT,..._\}\)：“\{ \}”中为此输入支持的数据类型的列表。

- OUTPUT\(_y_, TensorType\(\{_DT\_FLOAT,DT\_UINT8,..._  \}\)\)

    注册算子的输出信息。

  - y：宏参数，算子的输出名称。
  - TensorType\(\{_DT\_FLOAT,DT\_UINT8,..._  \}\)：“\{ \}”中为此输出支持的数据类型的列表。

    若算子有多个输出，每个输出需要使用一条OUTPUT\(_y,_  TensorType\(\{  _DT\_FLOAT,DT\_UINT8,..._  \}\)\)语句进行注册。

- DYNAMIC\_OUTPUT\(_c_, TensorType\(\{_DT\_FLOAT, DT\_INT32,..._\}\)\)

    算子为动态多输出场景下的输出信息注册。

  - c：宏参数，算子的输出名称，图运行时，会根据输出的个数自动生成c0、c1、c2……，序号依次递增。
  - TensorType\(\{_DT\_FLOAT,DT\_UINT32,..._  \}\)：“\{ \}”中为此输出支持的数据类型的列表。

- ATTR\(_x_, Type，_DefaultValue_\)

    注册算子的属性，包括算子的属性名称，属性类型以及属性值的默认值，当开发者不设置算子对象的属性值时，系统会使用默认值。

    例如：ATTR\(mode, Int, 1\)，注册属性mode，属性类型为int64\_t，默认值为1。

    若算子有多个属性，每个属性需要使用一条ATTR\(_x_, Type，_DefaultValue_\)语句进行注册。

- REQUIRED\_ATTR\(_d_, Type\)

    注册算子的属性，包括算子的属性名称与属性类型，无默认值，开发者必须设置算子对象的属性值。

    若算子有多个属性，每个属性需要使用一条REQUIRED\_ATTR\(_d_, Type\)语句进行注册。

- OP\_END\_FACTORY\_REG\(_OpType_\)：结束算子注册。OpType与REG\_OP\(_OpType_\)中的OpType保持一致。

>[!NOTE]说明
>
>DT\_FLOAT，DT\_UINT8_等数据类型对应关系请参见[《基础数据结构和接口》](https://hiascend.com/document/redirect/CannCommunitybasicopapi)\>"ge命名空间\>DataType"。

## 如何获取算子原型

在模型构建时，用户需要了解算子原型，包括输入、输出和属性信息，从而创建算子实例，构建自己的Graph。

- 对于用户自定义算子，请自行获取自定义算子的算子原型定义头文件，了解算子的原型定义。

    从`${INSTALL_DIR}/opp/vendors/<vendor_name>/op_proto/inc`算子原型定义的头文件中获取，例如：

    ```c++
    REG_OP(SubMConv3dCube)
        .INPUT(x, TensorType({DT_FLOAT16}))
        .INPUT(filter, TensorType({DT_FLOAT16}))
        .OUTPUT(y, TensorType({DT_FLOAT16}))
        .ATTR(is_first, Bool, false)
        .OP_END_FACTORY_REG(SubMConv3dCube)
    ```

- 对于内置算子，用户可以通过如下两种方式获取算子原型：

  - 从[《算子库》](https://hiascend.com/document/redirect/CannCommunityOplist)中的“Ascend IR算子规格说明”获取，如下图所示。

      **图 1**  查看算子原型信息
      ![算子原型信](../figures/view_op_prototype_info.png "查看算子原型信息")

  - 从`${INSTALL_DIR}/opp/built-in/op_graph/inc`算子原型定义的头文件中获取，例如：

      ```c++
      REG_OP(SoftmaxV2)
          .INPUT(x, TensorType({DT_DOUBLE, DT_FLOAT16, DT_BF16, DT_FLOAT}))
          .OUTPUT(y, TensorType({DT_DOUBLE, DT_FLOAT16, DT_BF16, DT_FLOAT}))
          .ATTR(axes, ListInt, {-1})
          .ATTR(half_to_float, Bool, false)
          .OP_END_FACTORY_REG(SoftmaxV2)
      ```

    其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。
