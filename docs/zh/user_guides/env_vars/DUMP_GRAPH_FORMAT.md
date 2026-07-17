# DUMP\_GRAPH\_FORMAT

## 功能描述

控制需要生成的dump文件类型。

支持的取值包括：

- onnx：基于ONNX的模型描述结构，可以使用Netron等可视化软件打开，生成文件名为ge\_onnx\*.pbtxt。
- ge\_proto：protobuf格式存储的文本文件，生成文件名为ge\_proto\*.txt。
- readable：类似Dynamo fx图风格的高可读性文本文件，生成文件名为ge\_readable\*.txt。文件内容解析请参见[readable文件解析](#readable文件解析)。

使用方式：

配置按照“|”分隔的字符串，类型全部为小写，支持组合配置，例如“ge\_proto|onnx|readable”，表示dump全量类型的文件；也支持单独配置，例如“ge\_proto”，表示只dump ge\_proto类型的文件ge\_proto\*.txt。

DUMP\_GRAPH\_FORMAT环境变量只有在[DUMP\_GE\_GRAPH](DUMP_GE_GRAPH.md)开启时才生效，默认为“ge\_proto|onnx”。

## 配置示例

```bash
export DUMP_GRAPH_FORMAT="ge_proto|onnx"
```

## 使用约束

- 如果此环境变量设置了其他非法值，可能会导致未定义的行为发生。
- 如果开启了采集算子dump数据功能，可以参考[ge.exec.enableDump](../../api/graph_engine_api/cpp/ge/options_params/precision_comparison.md)参数，即使DUMP\_GRAPH\_FORMAT环境变量取值不包括“ge\_proto”，最终都会dump子图ge\_proto\_xxxx\_Build.txt。

## readable文件解析

- 如下为一个完整的ge\_readable\*.txt文件：

    ```text
    graph("MakeTransformerSubGraph"):
      %input_0 : [#users=1] = Node[type=Data] (attrs = {index: 0})
      %input_1 : [#users=1] = Node[type=Data] (attrs = {index: 1})
      %input_2 : [#users=1] = Node[type=Data] (attrs = {index: 2})
      %Const_0 : [#users=1] = Node[type=Const] (attrs = {value: [-1 7168]})
      %Reshape_1 : [#users=1] = Node[type=Reshape] (inputs = (x=%input_0, shape=%Const_0), attrs = {axis: 0, num_axes: -1})
      %Cast_2 : [#users=1] = Node[type=Cast] (inputs = (x=%Reshape_1), attrs = {dst_type: 0})
      %Cast_3 : [#users=1] = Node[type=Cast] (inputs = (x=%input_1), attrs = {dst_type: 0})
      %Const_4 : [#users=1] = Node[type=Const] (attrs = {value: [1 0]})
      %Transpose_5 : [#users=1] = Node[type=Transpose] (inputs = (x=%Cast_3, perm=%Const_4))
      %MatMul_6 : [#users=1] = Node[type=MatMul] (inputs = (x1=%Cast_2, x2=%Transpose_5), attrs = {transpose_x1: false, transpose_x2: false})
      %Sigmoid_7 : [#users=1] = Node[type=Sigmoid] (inputs = (x=%MatMul_6))
      %Const_8 : [#users=1] = Node[type=Const] (attrs = {value: [-1 256]})
      %Reshape_9 : [#users=1] = Node[type=Reshape] (inputs = (x=%Sigmoid_7, shape=%Const_8), attrs = {axis: 0, num_axes: -1})
      %Unsqueeze_10 : [#users=1] = Node[type=Unsqueeze] (inputs = (x=%input_2), attrs = {axes: {0}})
      %Cast_11 : [#users=1] = Node[type=Cast] (inputs = (x=%Unsqueeze_10), attrs = {dst_type: 0})
      %Add_12 : [#users=1] = Node[type=Add] (inputs = (x1=%Reshape_9, x2=%Cast_11))
      %Const_13 : [#users=1] = Node[type=Const] (attrs = {value: [2]})
      %TopKV2_14 : [#users=2] = Node[type=TopKV2] (inputs = (x=%Add_12, k=%Const_13), attrs = {sorted: true, dim: -1, largest: true, indices_dtype: 3})
      %ret : [users=1] = get_element[node=%TopKV2_14](0)
      %ret_1 : [users=0] = get_element[node=%TopKV2_14](1)
      %Const_15 : [#users=1] = Node[type=Const] (attrs = {value: [-1]})
      %ReduceSum_16 : [#users=1] = Node[type=ReduceSum] (inputs = (x=%ret, axes=%Const_15), attrs = {keep_dims: false, noop_with_empty_axes: true})
      %Const_17 : [#users=1] = Node[type=Const] (attrs = {value: [4]})
      %TopKV2_18 : [#users=2] = Node[type=TopKV2] (inputs = (x=%ReduceSum_16, k=%Const_17), attrs = {sorted: false, dim: -1, largest: true, indices_dtype: 3})
      %ret_2 : [users=0] = get_element[node=%TopKV2_18](0)
      %ret_3 : [users=1] = get_element[node=%TopKV2_18](1)
      %Cast_19 : [#users=1] = Node[type=Cast] (inputs = (x=%ret_3), attrs = {dst_type: 9})
      %ZerosLike_20 : [#users=1] = Node[type=ZerosLike] (inputs = (x=%ReduceSum_16))
      %Shape_21 : [#users=1] = Node[type=Shape] (inputs = (x=%Cast_19), attrs = {dtype: 3})
      %Const_22 : [#users=1] = Node[type=Const] (attrs = {value: [1.000000]})
      %Cast_23 : [#users=1] = Node[type=Cast] (inputs = (x=%Const_22), attrs = {dst_type: 0})
      %Fill_24 : [#users=1] = Node[type=Fill] (inputs = (dims=%Shape_21, value=%Cast_23))
      %ScatterElements_25 : [#users=1] = Node[type=ScatterElements] (inputs = (data=%ZerosLike_20, indices=%Cast_19, updates=%Fill_24), attrs = {axis: 0, reduction: "none"})
      %Unsqueeze_26 : [#users=1] = Node[type=Unsqueeze] (inputs = (x=%ScatterElements_25), attrs = {axes: {-1}})
      %Const_27 : [#users=1] = Node[type=Const] (attrs = {value: [256 256]})
      %BroadcastTo_28 : [#users=1] = Node[type=BroadcastTo] (inputs = (x=%Unsqueeze_26, shape=%Const_27))
      %Identity_29 : [#users=1] = Node[type=Identity] (inputs = (x=%BroadcastTo_28))
      %Const_30 : [#users=1] = Node[type=Const] (attrs = {value: [256 256]})
      %Reshape_31 : [#users=1] = Node[type=Reshape] (inputs = (x=%Identity_29, shape=%Const_30), attrs = {axis: 0, num_axes: -1})
      %Cast_32 : [#users=1] = Node[type=Cast] (inputs = (x=%Reshape_31), attrs = {dst_type: 12})
      %LogicalNot_33 : [#users=1] = Node[type=LogicalNot] (inputs = (x=%Cast_32))
      %Const_34 : [#users=1] = Node[type=Const] (attrs = {value: [0.000000]})
      %MaskedFill_35 : [#users=1] = Node[type=MaskedFill] (inputs = (x=%Add_12, mask=%LogicalNot_33, value=%Const_34))
      %Const_36 : [#users=1] = Node[type=Const] (attrs = {value: [4]})
      %TopKV2_37 : [#users=2] = Node[type=TopKV2] (inputs = (x=%MaskedFill_35, k=%Const_36), attrs = {sorted: false, dim: -1, largest: true, indices_dtype: 3})
      %ret_4 : [users=0] = get_element[node=%TopKV2_37](0)
      %ret_5 : [users=1] = get_element[node=%TopKV2_37](1)
      %Cast_38 : [#users=1] = Node[type=Cast] (inputs = (x=%ret_5), attrs = {dst_type: 9})
      %GatherElements_39 : [#users=1] = Node[type=GatherElements] (inputs = (x=%Sigmoid_7, index=%Cast_38), attrs = {dim: 1})
      %Const_40 : [#users=1] = Node[type=Const] (attrs = {value: [0.000001]})
      %RealDiv_41 : [#users=1] = Node[type=RealDiv] (inputs = (x1=%GatherElements_39, x2=%Const_40))
      %Const_42 : [#users=1] = Node[type=Const] (attrs = {value: [2.500000]})
      %Mul_43 : [#users=1] = Node[type=Mul] (inputs = (x1=%RealDiv_41, x2=%Const_42))
      %Cast_44 : [#users=1] = Node[type=Cast] (inputs = (x=%Mul_43), attrs = {dst_type: 0})

      return (output_0=%Cast_38, output_1=%Cast_44)
    ```

- 如下为一个包含子图的文件：

    ```text
    graph("TransformerBlockSubgraph"):
      %input_0 : [#users=1] = Node[type=Data] (attrs = {index: 0})
      %pred_1 : [#users=1] = Node[type=Data] (attrs = {index: 1})
      %If_0 : [#users=1] = Node[type=If] (inputs = (cond=%pred_1, input_0=%input_0), attrs = {then_branch: %If_then, else_branch: %If_else})
      %Const_1 : [#users=1] = Node[type=Const] (attrs = {value: [0]})
      %Const_2 : [#users=1] = Node[type=Const] (attrs = {value: [4]})
      %Const_3 : [#users=1] = Node[type=Const] (attrs = {value: [1]})
      %For_6 : [#users=2] = Node[type=For] (inputs = (start=%Const_1, limit=%Const_2, delta=%Const_3, input_1=%If_0), attrs = {body: %For_body})
      %ret : [users=1] = get_element[node=%For_6](0)
      %ret_1 : [users=1] = get_element[node=%For_6](1)

      return (output_0=%ret, output_1=%ret_1)

    graph("If_then"):
      %input_0 : [#users=1] = Node[type=Data] (attrs = {index: 0})
      %Const_0 : [#users=1] = Node[type=Const] (attrs = {value: [0.900000]})
      %Mul_1 : [#users=1] = Node[type=Mul] (inputs = (x1=%input_0, x2=%Const_0))
      return (%Mul_1)

    graph("If_else"):
      %input_0 : [#users=1] = Node[type=Data] (attrs = {index: 0})
      %Identity_0 : [#users=1] = Node[type=Identity] (inputs = (x=%input_0))
      return (%Identity_0)

    graph("For_body"):
      %iter : [#users=1] = Node[type=Data] (attrs = {index: 0})
      %hidden : [#users=1] = Node[type=Data] (attrs = {index: 1})
      %Const_0 : [#users=1] = Node[type=Const] (attrs = {value: [1]})
      %Add_0 : [#users=1] = Node[type=Add] (inputs = (x1=%iter, x2=%Const_0))
      %Const_1 : [#users=1] = Node[type=Const] (attrs = {value: [0.500000]})
      %Mul_1 : [#users=1] = Node[type=Mul] (inputs = (x1=%hidden, x2=%Const_1))
      %Add_2 : [#users=1] = Node[type=Add] (inputs = (x1=%hidden, x2=%Mul_1))

      return (output_0=%Add_0, output_1=%Add_2)
    ```

字段解释如下：

- **图名称**：graph\("<图名称\>"\)。
- **节点实例**：%<节点实例名称\> : \[\#users=<出度\>\] = Node\[type=<节点类型\>\]\(inputs = \(<输入名称1\>=%<输入实例1\>, ...\), attrs = \{<属性名称1\>: <属性值1\>, ...\}\)。
  - <节点实例名称\>：节点实例名称。
  - \#users=<出度\> ：节点输出个数。
  - Node\[type=<节点类型\>\]：节点对应的算子类型，例如MatMul节点显示为Node\[type=MatMul\]。
  - inputs = \(<输入名称1\>=%<输入实例1\>, ...\)：节点输入以“参数名=实例名”形式展示；参数名解析异常时，回退为 \_input\_N 序列。当节点无输入时该字段缺省。

    **动态输入**：对动态输入参数使用输入名称\_\#cnt形式编号（输入名称\_0, 输入名称\_1, ...）。

  - attrs = \{<属性名称1\>: <属性值1\>, ...\}：节点属性集合，包含子图属性项与普通属性项，当属性为空时该字段缺省。

- **多输出节点的输出引用**：%ret/ret\_\#cnt : \[\#users=<消费者个数\>\] = get\_element\[node=%<节点实例名称\>\]\(<输出index\>\)。
  - %ret/ret\_\#cnt：多输出节点的每一路输出统一使用ret命名ret, ret\_1, ret\_2, ...。
  - \[\#users=<消费者个数\>\]：该输出的消费者数量。
  - get\_element\[node=%<节点实例名称\>\]\(<输出index\>\)：以get\_element表示“从多输出节点提取第 <输出index\> 路输出”。

- **图输出**：return \(<输出列表\>\)
- 表示图的输出，对应NetOutput节点的输入。
  - 单输出：return \(%<输出实例\>\)。
  - 多输出：return \(output\_0=%<输出实例0\>, output\_1=%<输出实例1\>, ...\)。输出使用output\_\#cnt形式编号（output\_0, output\_1, ...）。

- **子图表示**：包含子图的节点按以下规则表示：
  - **子图声明**：在父节点的attrs中声明：attrs = \{<子图属性名称\>: %<子图实例名\>, ... \}；子图属性名称解析异常时，回退为\_graph\_N序列。
  - **输入对应关系**：父节点的输入input\_\#cnt（或 args\_\#cnt）对应子图中index属性值为cnt的Data节点。例如：父节点的input\_0 →子图的Data\(attrs = \{index: 0\}\)。
  - **输出对应关系**：
    - **单输出**：子图返回值直接作为父节点输出。
    - **多输出**：子图的输出output\_\#cnt对应父节点的第cnt路输出，父节点通过get\_element\[node=%<父节点\>\]\(cnt\)提取对应输出。

  - **子图展示位置**：子图内容在父图主体输出结束后单独展示；每个子图以 graph\("<子图实例名\>"\):开始。

## 产品支持情况

全量芯片支持
