# Readable Dump Documentation

## Overview

Readable Dump is a high-readability graph structure export format provided by GE (Graph Engine), using text representation similar to Dynamo fx graph style, facilitating developers to directly read and understand computation graph structures.

## Design Principles

**Readability as highest priority, simplicity over completeness**:

- **Display method**: Through function-call-like approach, display IR attributes, inputs, outputs information
- **Information filtering**: Non-IR defined information like control edges and private attributes not displayed (affects overall readability)
- **Value abbreviation**: Values carried by Const use abbreviated display, see [Appendix: Const Constant Display Rules](#const-constant-display-rules)

---

## readable File Parsing

Example 1: Below is a complete ge_readable*.txt file:
```
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

Example 2: Below is a file containing subgraphs:
```
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

Field explanations:
- **Graph name**: `graph("<graph_name>")`.
- **Node instance**: `%<node_instance_name> : [#users=<out_degree>] = Node[type=<node_type>](inputs = (<input_name1>=%<input_instance1>, ...), attrs = {<attribute_name1>: <attribute_value1>, ...})`.
    - `%<node_instance_name>`: Node instance name.
    - `[#users=<out_degree>]`: Number of node outputs.
    - `Node[type=<node_type>]`: Operator type corresponding to the node, for example MatMul node displays as `Node[type=MatMul]`.
    - `inputs = (<input_name1>=%<input_instance1>, ...)`:
       Node inputs are displayed in "parameter_name=instance_name" format; when parameter name parsing fails, it falls back to `_input_N` sequence. This field is omitted when the node has no inputs.
       - **Dynamic inputs**: Dynamic input parameters use `input_name_#cnt` numbering format (`input_name_0, input_name_1, ...`).
    - `attrs = {<attribute_name1>: <attribute_value1>, ...}`:
       Node attribute collection, containing **subgraph attribute items** and regular attribute items. This field is omitted when attributes are empty.
- **Multi-output node output reference**: `%ret/ret_#cnt : [#users=<consumer_count>] = get_element[node=%<node_instance_name>](<output_index>)`.
    - `%ret/ret_#cnt`: Each output of a multi-output node uses the unified ret naming `ret, ret_1, ret_2, ...`.
    - `[#users=<consumer_count>]`: Number of consumers for this output.
    - `get_element[node=%<node_instance_name>](<output_index>)`: Uses `get_element` to represent "extract the `<output_index>`-th output from a multi-output node".
- **Graph output**: `return (<output_list>)`  
  Represents graph output, corresponding to `NetOutput` node inputs.
    - **Single output**: `return (%<output_instance>)`.
    - **Multiple outputs**: `return (output_0=%<output_instance0>, output_1=%<output_instance1>, ...)`. Outputs use `output_#cnt` numbering format (`output_0, output_1, ...`).
- **Subgraph representation**:  
  Nodes containing subgraphs are represented according to the following rules:
    - **Subgraph declaration**: Declared in parent node's `attrs`: `attrs = {<subgraph_attribute_name>: %<subgraph_instance_name>, ... }`; when subgraph attribute name parsing fails, it falls back to `_graph_N` sequence.
    - **Input correspondence**: Parent node input `input_#cnt` (or `args_#cnt`) corresponds to the `Data` node with `index` attribute value of `cnt` in the subgraph.  
      For example: Parent node's `input_0` → Subgraph's `Data(attrs = {index: 0})`.
    - **Output correspondence**:
      - **Single output**: Subgraph return value directly serves as parent node output.
      - **Multiple outputs**: Subgraph output `output_#cnt` corresponds to parent node's `cnt`-th output. Parent node extracts corresponding output via `get_element[node=%<parent_node>](cnt)`.
    - **Subgraph display position**:  
      Subgraph content is displayed separately after the main parent graph output ends; each subgraph starts with `graph("<subgraph_instance_name>"):`.

---

## Appendix

### Const Constant Display Rules

To balance readability and information completeness, the `value` attribute of Const nodes is displayed with the following abbreviation rules:

- **No value attribute**: Not displayed
- **Empty tensor**: Indicates no data content, displayed as `value: <empty>`
- **Single value**: Represents scalar `[]` or single element `[1]`, displays the actual value directly, e.g. `value: [1.5]`
- **Multiple values**: When values exceed 6, use abbreviated form keeping first 3 and last 3 values, e.g. `value: [1 2 3 ... 98 99 100]`
- **Unsupported data type**: Displayed as `value: <not_supported>`