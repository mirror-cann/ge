# GE IR Implementation Tutorial

GE IR (Intermediate Representation) is used to translate from user model expression to executable code, and is also a specification description of a operator's computational semantics. This document will introduce the definition of GE IR and related development work.

## Deliverables for Implementing IR

In runtime2, the IR implementation for first and second type operators needs to include the following parts:

* DataType inference (mandatory)
* Shape inference (mandatory)
* ShapeRange inference (optional)
* Tiling functionality (mandatory)
* Declare data dependencies (optional)

## Operator Classification

Based on the differences in shape inference and dispatch logic, operators are divided into four categories.

| Classification | Characteristics                                                         | Optional Deliverable Requirements                            |
| ---- | ------------------------------------------------------------ |------------------------------------|
| Type 1 | Output shape can be inferred from input shape.                           | 1.InferShpeRange(as needed)               |
| Type 2 | Output shape depends on input tensor value. Like Reshape operator, which depends on shape input value to infer output shape. This type is also called value-dependent operator. | 1.InferShpeRange(as needed)<br />2.Declare data dependency |
| Type 3 | Cannot infer output shape at compile time, only can infer output shape range. Output shape is obtained after execution. Like Where operator.<br />During dispatch, need to allocate maximum output memory according to output shape range, so InferShpeRange is mandatory deliverable | 1.InferShpeRange<br />2.Declare data dependency(as needed) |
| Type 4 | Cannot infer shape. Also don't know output shape range. Like aicpu fusion operators, some resource operators. These operators usually call tf native operator implementations, so no IR deliverables needed. | NA                                 |

## Registration

Take `Cast` operator as an example, register operator implementation as follows:

```C++
#include "register/op_impl_registry.h"  // Header file needed for IMPL_OP

// Implement `InferShape` function
ge::graphStatus InferShapeForCast(InferShapeContext *context) {
  // TODO
  return ge::GRAPH_SUCCESS;
}

IMPL_OP(Cast)          // Use `IMPL_OP` to register operator implementation, this macro accepts one parameter, which is the operator type to be registered
    .InferShape(InferShapeForCast);   // Register InferShape function
```

## Implement InferDataType

### 1.IR Express Inference Logic

Currently supports declarative datatype inference based on prototype, using generalized types in prototype definition to define input/output datatype mapping relationship.

Take Add operator as an example, when defining prototype, declare datatype inference rules as follows:

```c++
REG_OP(Add)
    .INPUT(x1, "T")
    .INPUT(x2, "T")
    .OUTPUT(y, "T")
    .OP_END_FACTORY_REG(Add)
```

The string "T" after x1, x2, y represents the datatype type of this input/output, this declaration expresses two meanings:

1. Add operator's two inputs must have the same datatype
2. Add operator's output type is consistent with input type



Sometimes operators have certain requirements for input/output datatypes, for example Add operator only supports calculation on real numbers, but doesn't support complex resource-type data like DT_RESOURCE, at this time can use `DATATYPE` to limit the range of type T:

```c++
REG_OP(Add)
    .INPUT(x1, "T")
    .INPUT(x2, "T")
    .OUTPUT(y, "T")
    .DATATYPE(T, TensorType::NumberType())  // NumberType includes INT, FLOAT etc. datatypes, details in "graph/types.h"
    .OP_END_FACTORY_REG(Add)
```



When input is DYNAMIC_INPUT, two possibilities will occur:

* All datatype types are consistent after this DYNAMIC_INPUT instantiation[^1]
* After this DYNAMIC_INPUT instantiation, there is no datatype constraint relationship between inputs



DATATYPE macro used with T syntax, represents the valid range of this datatype, its datatype can only be TensorType or ListTensorType. If T syntax is used but DATATYPE not defined, then only datatype mapping is done, abandoning datatype verification.

Below are common datatype inference operator prototype definition examples.

#### 1.1.Output inferred from input

Take StridedSlice operator as an example.

Output y's datatype is consistent with input x, so they use the same symbol T. T's datatype optional range is TensorType::BasicType(), declared by DATATYPE macro.

begin, end, strides input datatypes are consistent, all use TIndex symbol, its datatype range is TensorType::IndexNumberType().

```c++
REG_OP(StridedSlice)
    .INPUT(x, "T")
    .INPUT(begin, "TIndex")
    .INPUT(end, "TIndex")##### 1.1.1 Usage of ListTensorType

ListTensorType can also be used to describe the value range corresponding to a symbol, in this case the input/output corresponding to this symbol should be dynamic. And it means the instance datatype value range of this dynamic input/output is the same, but datatypes don't have to be the same. For example IdentityN.

```c++
REG_OP(IdentityN)
    .DYNAMIC_INPUT(x, "T")
    .DYNAMIC_OUTPUT(y, "T")
    .DATATYPE(T, ListTensorType({DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT16, DT_UINT16, DT_UINT8,
        DT_INT32, DT_INT64, DT_UINT32, DT_UINT64, DT_BOOL, DT_DOUBLE}))
    .OP_END_FACTORY_REG(IdentityN)
```

Another example of dynamic input but not using ListTensorType, ConcatV2. Output y datatype is consistent with input x datatype, and input_x0...input_xi datatypes are all consistent, in this case T still uses TensorType representation.

```c++
REG_OP(ConcatV2)
    .DYNAMIC_INPUT(x, "T")
    .INPUT(concat_dim, TensorType::IndexNumberType())
    .OUTPUT(y, "T")
    .ATTR(N, Int, 1)
    .DATATYPE(T, TensorType::BasicType())
    .OP_END_FACTORY_REG(ConcatV2)
```

> WARNING: If an input's DataType is determined by an optional input, then when optional input doesn't exist, that output's DataType cannot be inferred, therefore need to consider scenarios where optional input doesn't exist. Generally, such operators are recommended to write custom inference rules (see custom inference chapter)

#### 1.2. Output Derived from Attribute

Take Cast operator as example, `Cast` operator converts input Tensor's data type to `dst_type` specified by attribute. Belongs to attribute-specified output datatype inference type. Cast operator prototype should be defined as:

```c++
REG_OP(Cast)
    .INPUT(x, "T")
    .OUTPUT(y, "dst_type")
    .REQUIRED_ATTR(dst_type, Int)
    .DATATYPE(T, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8, DT_INT64, DT_UINT64, DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16, DT_QUINT16, DT_QINT32, DT_BF16}))
    .DATATYPE(dst_type, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8, DT_INT64, DT_UINT64, DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16, DT_QUINT16, DT_QINT32, DT_BF16}))
    .OP_END_FACTORY_REG(Cast)
```

As shown in above IR, output y's datatype is mapped from dst_type symbol to attribute dst_type. Where dst_type's data type range is limited by DATATYPE macro.

#### 1.3. Output with Fixed datatype

Take Equal operator as example, its output is fixed as DT_BOOL. Then just specify output symbol's TensorType with only one value DT_BOOL.

```c++
REG_OP(Equal)
    .INPUT(x1, "TIn")
    .INPUT(x2, "TIn")
    .OUTPUT(y, "TOut")
    .DATATYPE(TIn, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8,DT_INT64, DT_UINT64, DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64,DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16, DT_QUINT16, DT_QINT32})
    .DATATYPE(TOut, TensorType({DT_BOOL}))
    .OP_END_FACTORY_REG(Equal)
```

#### 1.4. Type Promotion Rules (todo)

### 2. Custom Inference Function

For custom inference, cannot express its mapping relationship on IR, please register and implement infer data type interface, if operator self-registered infer data type interface, then that interface will be called first.

```c++
ge::graphStatus InferDataTypeForFoo(gert::InferDataTypeContext* context) {
    // TODO Implement custom inference rules
    if (context->GetInputDataType(0) == DT_INT4) {
        context->SetOutputDataType(0, DT_INT32);
    }
}

IMPL_OP(Foo).InferDataType(InferDataTypeForFoo);// Registration
```

## Implement InferShape

Take `Cast` operator as example, `Cast` operator converts input Tensor's data type to `dst_type` specified by attribute, `Cast` operator prototype is:

```c++
REG_OP(Cast)
    .INPUT(x, "T")
    .OUTPUT(y, "dst_type")
    .REQUIRED_ATTR(dst_type, Int)
    .DATATYPE(T, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8, DT_INT64, DT_UINT64, DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16, DT_QUINT16, DT_QINT32, DT_BF16}))
    .DATATYPE(dst_type, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8, DT_INT64, DT_UINT64, DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16, DT_QUINT16, DT_QINT32, DT_BF16}))
    .OP_END_FACTORY_REG(Cast)
```

From `Cast` operator's functionality, its output Shape equals input Shape, InferShape function implementation is:

```c++
ge::graphStatus InferShapeForCast(InferShapeContext *context) {
  // Get pointers to 0th input and output
  const gert::Shape *shape = context->GetInputShape(0);
  gert::Shape *output_shape = context->GetOutputShape(0);
  if (shape == nullptr || output_shape == nullptr) {
    // Defensive programming, scenario that shouldn't appear, print error and return failure
  GELOGE(ge::PARAM_INVALID,
         "Failed to infer shape for node %s, type %s, nullptr found",
         context->GetNodeName(),
         context->GetNodeType());
    return ge::GRAPH_FAILED;
  }

  *output_shape = *shape;  // Assign input shape to output shape

  return ge::GRAPH_SUCCESS;
}
```

InferShape function prototype is definite, it accepts one `InferShapeContext` as input, from this context, can get input, output shape pointers etc. After InferShape succeeds, returns ge::GRAPH_SUCCESS, other return values are considered inference failure. After inference fails, execution process ends and exits.

From above code, input shape is const, therefore when InferShape, input shape is read-only, not allowed to modify.

`InferShapeContext` publicly inherits from `ExtendedKernelContext`, therefore methods provided in `ExtendedKernelContext` such as getting operator type, name, attributes etc interfaces can all be called in `InferShapeContext` instance. Detailed interfaces of `InferShapeContext` and `ExtendedKernelContext` refer to "runtime2.0 API specification".

> WARNING: For efficiency consideration, when calling InferShape function, framework won't initialize output shape, therefore in InferShape function, can consider output is **uninitialized** state. If in InferShape, hope to operate output Shape via Append way, need to first clear output Shape's DimNum to zero, to prevent undefined behavior.

## Implement InferShapeRange

Three categories of operators InferShapeRange is mandatory deliverable.

For category 1 and 2 operators, when their InferShapeFunc satisfies following conditions, can use framework automatic inference mechanism, no need to provide custom InferShapeRange.

1. Single input single output operator, InferShapeFunc satisfies monotonicity. Monotonicity means, in given interval, given input x value, output y value is unique. If doesn't satisfy monotonicity, automatic inference result will be inaccurate.
2. Multi input multi output operator, because automatic inference framework needs to select input shape range min and max values to feed into InferShape function, for multi input operators, different combinations of different inputs' different shapes all need to be legal.

Therefore operator developers need to judge based on operator InferShape's mathematical semantics whether need to provide custom InferShapeRange function. If because operator didn't provide InferShapeRange function, leading to inference result error, can only error at subsequent execution time.

### 1. Custom InferShapeRange

Take Where operator as example, its IR definition is shown below.

```c++
/**
* @brief Returns locations of nonzero / true values in a tensor. \n

* @par Inputs:
* Including:
* @li x: A Tensor. Must be one of the following types:
  DT_DOUBLE, DT_FLOAT, DT_FLOAT16, DT_INT8, DT_UINT8, DT_QINT8,
  DT_QUINT8, DT_INT16, DT_UINT16, DT_INT32, DT_UINT32, DT_QINT32,
  DT_INT64, DT_UINT64, DT_BOOL, DT_COMPLEX64, DT_COMPLEX128 \n

* @par Outputs:
* @li y: A Tensor of type DT_INT64. \n

* @attention Constraints:
* Where runs on the Ascend AI CPU, which delivers poor performance.\n

* @par Third-party framework compatibility
* Compatible with the TensorFlow operator Where.
*/

REG_OP(Where)
    .INPUT(x, TensorType({BasicType(), DT_BOOL}))
    .OUTPUT(y, TensorType({DT_INT64}))
    .OP_END_FACTORY_REG(Where)
```

Where operator semantics is, returns coordinates of non-zero/True values in a Tensor.

Where's output y_shape will be [-1, Rank(x_shape)]. Where first axis value depends on non-zero value count, second axis value depends on x_shape dimension.

For example: Input x is a 4-dimensional Tensor, input shape is [4, 5, 6, 7]. Output shape is [-1, 4].

Then y_shape's first axis value, minimum is 0, maximum is value count in input x (i.e., x_shape_size, product of x shape each axis).

InferShapeRange implementation is:```c++
  // If input x's max range unknown, then output y's shape max range also unknown
  if (x_max_shape->GetDim(i) < 0) {
    output_shape_max = -1;
    break;
  }
  // If input x's max shape range is known, output y's shape max range will be x's max shape size
  output_shape_max *= x_max_shape->GetDim(i);
}

// Set output range
output_shape_range->GetMax()->SetDimNum(output_shape_dim_num);
output_shape_range->GetMax()->SetDim(0, output_shape_max);
output_shape_range->GetMax()->SetDim(1, x_shape_dimnum);
output_shape_range->GetMin()->SetDimNum(output_shape_dim_num);
output_shape_range->GetMin()->SetDim(0, 0);
output_shape_range->GetMin()->SetDim(1, x_shape_dimnum);

return ge::GRAPH_SUCCESS;
}
```

### 2. Framework Automatic Inference Mechanism

- If operator input has shape range, substitute shape range's min/max values, call static infer func. Compare output shape axis by axis, if dim different, represent as dynamic axis. Output shape derived from min/max inference serves as output shape range's min/max.
- If operator input doesn't have shape range, only has dynamic axis information. Skip shape range inference.

## Implement Tiling

Tiling function is divided into two steps: TilingParse and Tiling. In TilingParse, deserialize Json generated after operator compilation, parse into custom CompileInfo data structure. Since deserialization time is long, TilingParse only executes once during loading phase, its result CompileInfo will be cached, passed to Tiling function for use during execution.

Take TransData operator as example, TilingParse and Tiling function prototype and registration method are shown below:

```c++
#include "register/op_impl_registry.h"  // Include header file needed by IMPL_OP

// TransData's CompileInfo definition
struct TransDataCompileInfo {
  int64_t ub_size;
  int64_t block_dim;
  int64_t group;
  int64_t vnc_fp32_flag;
};

// TilingParse prototype, input is KernelContext
ge::graphStatus TilingPrepareForTransdata(TilingParseContext *context) {
  return ge::GRAPH_SUCCESS;
}

// Tiling prototype, input is TilingContext
ge::graphStatus TilingForTransData(TilingContext *context) {
  return ge::GRAPH_SUCCESS;
}

IMPL_OP(TransData)
    .InferShape(InferShapeForTransData)
    .Tiling(TilingForTransData)                                     // Register Tiling function
    .TilingParse<TransDataCompileInfo>(TilingPrepareForTransdata);  // Register TilingParse function
```

Since each operator's CompileInfo type is different, when calling `TilingParse` registration, pass `CompileInfo` class as template parameter, `CompileInfo` class needs to provide default constructor.

### Implement TilingParse

TilingParse accepts a compiled json string as input, deserialize and parse this json, and write parsing result to its output, take TransData's Tiling as example:

```c++
ge::graphStatus TilingPrepareForTransdata(TilingParseContext *context) {
  // Get compiled json string
  const char *json_str = context->GetCompiledJson();
  if (compile_info == nullptr || json_str == nullptr) {
      GELOGE(ge::PARAM_INVALID,
         "Failed to parse compiled info for node %s, type %s, nullptr found",
         context->GetNodeName(),
         context->GetNodeType());
    return ge::GRAPH_FAILED;
  }

  // TilingPrepare only has one output, which is CompileInfo instance.
  // CompileInfo instance has already been created by framework according to type declared at TilingParse registration time, here just get it directly, no need to apply yourself
  TransDataCompileInfo *compile_info = context->GetCompiledInfo<TransDataCompileInfo>();

  // Use nlohmann library to deserialize json
  std::unique_ptr<nlohmann::json> parsed_object_cinfo(new nlohmann::json(nlohmann::json::parse(json_str)));
  if (parsed_object_cinfo == nullptr) {
    return ge::GRAPH_FAILED;
  }

  // Parse json, and write parsed result into CompileInfo instance
  const nlohmann::json &allVars = (*parsed_object_cinfo)["vars"];
  if (!GetCompileValue(allVars, "ub_size", compile_info->ub_size)) {
    GELOGE(ge::PARAM_INVALID, "Failed to find ub_size in json %s", json_str);
    return ge::PARAM_INVALID;
  }
	// More parsing logic...

  return ge::GRAPH_SUCCESS;
}
```

### Implement Tiling Function

Tiling function's parameter is `TilingContext`, this class inherits from `ExtendedKernelContext`, therefore this context inherits following capabilities from `ExtendedKernelContext`:

* This operator's IR attributes
* This operator's all inputs, outputs' original Format, runtime Format
* This operator's all inputs, outputs' runtime DataType
* This operator's inputs' instantiation information (about instantiation definition, refer to description in InferShape)

Besides, TilingContext provides extra data needed by Tiling function:

* This operator's all inputs, outputs' original shape, runtime shape
* CompileInfo instance

Tiling function needs to produce following outputs:

* tiling-key: default is 0
* block-dim: default is 0
* atomic-clean-flag: default is true
* tiling-data: default is empty
* workspaces size: default workspace count is 0

If Tiling function doesn't assign values to outputs, then outputs will use above default values.

Since Tiling logic is relatively complex, we only example methods of operating data structures in Tiling:

```c++
struct TransDataMode1010Param {
  int64_t tiling_mode;
  int64_t ub_offset;
  int64_t used_core_cnt;
  int64_t core_step_in;
  int64_t core_step_out;
  // More TilingData parameters
};

ge::graphStatus TilingForTransData(TilingContext *context) {
  // Get input output shapes, this shape contains original shape and runtime shape, detailed API refer to StorageShape definition
  const StorageShape *in_shape = context->GetInputShape(0);
  const StorageShape *out_shape = context->GetOutputShape(0);
  // Get CompileInfo instance, this instance is written by TilingParse
  auto compile_info = reinterpret_cast<const TransDataCompileInfo *>(context->GetCompileInfo());

  // Get input output TensorDesc, TensorDesc contains format information, DataType information
  const CompileTimeTensorDesc *src_td = context->GetInputDesc(0);
  const CompileTimeTensorDesc *dst_td = context->GetOutputDesc(0);
  if (in_shape == nullptr || out_shape == nullptr || src_td == nullptr || dst_td == nullptr ||
      compile_info == nullptr) {
    GELOGE(ge::PARAM_INVALID,
           "Failed to tiling for node %s, type %s, nullptr found",
           context->GetNodeName(),
           context->GetNodeType());
    return ge::FAILED;
  }
  ge::Format src_format = src_td->GetStorageFormat();
  ge::Format dst_format = dst_td->GetStorageFormat();

  // Generally, Tiling business logic is relatively complex, here only example interface usage method
  ASSERT_SUCCESS(context->SetBlockDim(32));  // Set BlockDim to 32
  ASSERT_SUCCESS(context->SetTilingKey(10)); // Set TilingKey to 10
  ASSERT_SUCCESS(context->SetNeedAtomic(false)); // Set no need for Atomic zero clearing

  size_t *workspaces_size = context->GetWorkspaceSizes(2);  // Need two workspaces
  ASSERT_NOT_NULL(workspaces_size);
  workspaces_size[0] = 1024;  // 1st workspace size is 1024
  workspaces_size[1] = 2048;  // 2nd workspace size is 2048

  // Set tiling data
  auto tiling_data = context->GetTilingData<TransDataMode1010Param>();
  if (tiling_data == nullptr) {
    return ge::FAILED;
  }
  tiling_data->tiling_mode = 10;
  tiling_data->ub_offset = 20;
  // ... More tiling_data assignment operations
  return ge::GRAPH_SUCCESS;
}
```

Similar to TilingParse, Tiling function output's required memory has already been pre-allocated by framework.

Framework will apply corresponding TilingData memory according to `OpDesc` attribute `op_para_size`, for AtomicClean operator, this attribute is marked on AtomicClean's host node's OpDesc, attribute name is `atomic_op_para_size`, this attribute is generated during operator compilation, and set on `OpDesc` by corresponding engine.

Current workspace maximum count supports 16, subsequently will create workspace size on demand according to TaskDef description.

### Append Way to Operate TilingData

TilingData has two operation methods, previous section introduced fixed data structure way, this way's characteristic is TilingData's memory layout is definite, therefore can abstract TilingData as a structure `Foo`, after getting corresponding class instance via `GetTilingData<Foo>()`, Tiling function directly writes data into instance. Another way is Append way, Append way's characteristic is TilingData's memory layout is indefinite, cannot abstract as a specific structure, at this time can treat TilingData as a binary stream, through continuous Append operations, append data to stream back, this section introduces Append way writing method.

```c++
struct Foo {
  int64_t a;
  int32_t b;
  int32_t c;
};

ge::graphStatus ExampleAppendTilingData(TilingContext *context) {
  TilingData *tiling_data = context->GetRawTilingData();  // Append way write, get RawTilingData
  ASSERT_NOT_NULL(tiling_data);

  // Add an int64_t type data to TilingData tail, an int32_t data, value is 10, after two Appends, TilingData length is 8+4=12// Append interface calculates added data length through data type, therefore when uncertain about C++ literal value constant default type, can explicitly specify Append data type through static_cast way
  tiling_data->Append(static_cast<int64_t>(10));
  tiling_data->Append(static_cast<int32_t>(10));

  // Append way also supports structure, compared to Append each member variable in structure separately, Append whole structure once has simpler encoding, also has better performance
  tiling_data->Append<Foo>({10, 20, 30});
  Foo foo{100, 200, 300};
  tiling_data->Append(foo);

  return ge::GRAPH_SUCCESS;
}
```



> **WARNING**: Fixed data structure, Append two operation methods are mutually exclusive, in one Tiling function, only can choose one way to write TilingData. During one Tiling process, simultaneously calling context->GetTilingData\<Foo>() and context->GetRawTilingData() two interfaces basically means bug produced.
>
> Fixed data structure, Append two operation methods mutual exclusion reason is tiling-data-size calculation, when calling context->GetTilingData\<Foo>(), framework will automatically set tiling-data-size to sizeof(Foo), while calling context->GetRawTilingData(), framework considers in Append mode, won't modify tiling-data-size, but does tiling-data-size accumulation action at each Append. Imagine a scenario: first call GetRawTilingData, and fill in an int64 data, at this time tiling-data-size becomes 8, then call context->GetTilingData\<int32_t>(), at this time tiling-data-size will be changed to sizeof(int32_t), first step Append's int64 data is overwritten

## Get Attributes

During InferShape, Tiling, can get operator IR attribute values through context instance, so-called IR attributes refer to attributes defined during IR registration, take `TransData` operator as example:
```c++
REG_OP(TransData)
    .INPUT(src, TensorType::BasicType())
    .OUTPUT(dst, TensorType::BasicType())
    .REQUIRED_ATTR(src_format, String)
    .REQUIRED_ATTR(dst_format, String)
    .ATTR(group, Int, 1)
    .OP_END_FACTORY_REG(TransData)
```

IR definition declares `src_format`, `dst_format`, `group` three attributes, can get operator attributes through following ways:

```c++
ge::graphStatus ExampleGetTransDataAttr(TilingContext *context) {
  // Get all attributes
  const RuntimeAttrs *attrs = context->GetAttrs();
  ASSERT_NOT_NULL(attrs);

  // According to order in IR definition, use index to get attribute, index counts from 0
  const char *src_format = attrs->GetAttrPointer<char>(0);  // Get src_format, src_format is first attribute in IR, therefore index is 0
  const char *dst_format = attrs->GetAttrPointer<char>(1);  // Get dst_format, dst_format is second attribute in IR, therefore index is 1
  const int64_t group = attrs->GetAttrPointer<int64_t>(2);  // Get group, group is third attribute in IR, therefore index is 2

  return ge::GRAPH_SUCCESS;
}
```

### Private Attributes

Some operators during InferShape or Tiling, may need to get some extra attribute values not defined in IR. Need to get such attribute values possibly for two purposes:

* IR definition incomplete, IR definition cannot completely describe this operator's computation logic, therefore needs extra attribute description
* Avoid repeated computation, some computations are time-consuming, and won't change, therefore can compute once during compilation

Corresponding to publicly defined attributes like IR attributes, we call such attributes **private attributes**. According to current lowering rules, only IR attributes will be retained, therefore private attributes will be discarded. If hope to also get private attributes during execution, through following way, declare at IMPL_OP:

```c++
// Register two private attributes for Foo operator, respectively `Bar1` and `Bar2`
IMPL_OP(Foo)
    .PrivateAttr("Bar1")
    .PrivateAttr("Bar2")
```

Private attribute get interface is consistent with IR attribute, where index parameter continues from IR attribute end index. Assuming in example `Foo` prototype is as shown below:

```c++
REG_OP(Foo)
    .INPUT(x, TensorType::BasicType())
    .OUTPUT(y, TensorType::BasicType())
    .REQUIRED_ATTR(ir_attr1, String)
    .REQUIRED_ATTR(ir_attr2, String)
    .ATTR(ir_attr3, Int, 1)
    .OP_END_FACTORY_REG(Foo)
```

From Foo prototype, this operator has 3 prototype attributes, therefore its IR attribute index range is [0,2], then `Foo` operator's two private attributes index range is [3,4], assuming two private attributes data types are both `int64_t`, then during InferShape can get through following way:

```C++
ge::graphStatus InferShapeForFoo(InferShapeContext *context) {
  // Get all attributes
  const RuntimeAttrs *attrs = context->GetAttrs();
  ASSERT_NOT_NULL(attrs);

  const int64_t attr1 = attrs->GetInt(3);  // Get Bar1 attribute, Bar1 is first private attribute after 3 IR attributes, therefore index is 3
  const int64_t attr2 = attrs->GetInt(4);  // Get Bar2 attribute, Bar2 is second private attribute after 3 IR attributes, therefore index is 4

  // Some other processing...

  return ge::GRAPH_SUCCESS;
}
```

> **WARNING:**
>
> 1. Although IMPL_OP itself allows same operator's different implementations to be defined in different files, but private attribute declaration must be completed in same file, otherwise we cannot confirm private attribute index
> 2. Once private attribute registered, then must be able to get successfully on Node, otherwise lowering errors, program exits. To ensure private attribute index is correct, framework cannot skip processing for unobtainable private attributes.

## Optional/Dynamic Input

In Runtime2.0, index instead of string name is used to index input output, this change is to reduce string matching during execution, improve execution efficiency. One problem brought by index matching is, for operators with OPTIONAL, DYNAMIC type inputs, after instantiation[^1], problem may appear that simply through index cannot index to specific input, take `DynamicRNNV3` operator as example:

```c++
REG_OP(DynamicRNNV3)
    .INPUT(x, TensorType({DT_FLOAT16, DT_FLOAT}))
    .INPUT(w, TensorType({DT_FLOAT16, DT_FLOAT}))
    .INPUT(b, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OPTIONAL_INPUT(seq_length, TensorType({DT_INT32}))
    .OPTIONAL_INPUT(init_h, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OPTIONAL_INPUT(init_c, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OPTIONAL_INPUT(wci, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OPTIONAL_INPUT(wcf, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OPTIONAL_INPUT(wco, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OPTIONAL_INPUT(mask, TensorType({DT_UINT8}))
    .OPTIONAL_INPUT(real_mask, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OPTIONAL_INPUT(project, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OUTPUT(y, TensorType({DT_FLOAT16, DT_FLOAT}))
		// Omit some OUTPUT definitions
    .ATTR(cell_type, String, "LSTM")
		// Omit some attribute definitions
    .OP_END_FACTORY_REG(DynamicRNNV3)
```

Since `DynamicRNNV3` operator has consecutive multiple optional inputs, this leads to `init_h` and inputs after it all have uncertain index after instantiation, for this type of operator, can get corresponding Shape etc data through IR definition Index, take InferShape as example:

```c++
ge::graphStatus InferShapeForDynamicRNNV3(InferShapeContext *context) {
  // For first two inputs, not affected by optional or dynamic, can get input shape by conventional method
  auto x_shape = context->GetInputShape(0);
  auto w_shape = context->GetInputShape(1);
  if (x_shape == nullptr || w_shape == nullptr) {
    return ge::GRAPH_FAILED;
  }

  int64_t state_size = 0;
  // On IR prototype, project is 11th input (counting from 0)
  constexpr int64_t kProjectInputIndex = 11;

  // Affected by previous optional inputs, project input index after instantiation is uncertain, get corresponding input shape through `GetOptionalInputShape`,
  // `GetOptionalInputShape` input parameter is `ir_index`, i.e., corresponding index on prototype
  auto project_shape = context->GetOptionalInputShape(kProjectInputIndex);
  if (project_shape != nullptr) {
    if (project_shape->GetDimNum() < 2) {
      return ge::GRAPH_FAILED;
    }
    state_size = project_shape->GetDim(1);
  }
  // More infershape logic...
  return ge::GRAPH_SUCCESS;
}
```

For dynamic type inputs, instantiated input may be one to multiple, for such inputs, get method is:

```c++
// ir_index: This input's index in IR definition, counting from 0
// relative_index: This input's relative index after instantiation, counting from 0, for example some DYNAMIC_INPUT instantiated 3, want to get second one, then relative_index = 1
auto shape = context->GetDynamicInputShape(ir_index, relative_index);
```

For operators with many inputs, getting input by index during execution appears not friendly enough, best to be able to automatically generate corresponding macro or constant during IR registration, for example project input index is predefined as `op::DynamicRNNV3::kIrInputProject`, this feature will be completed later, please look forward to it.

Optional, dynamic input get ways exemplified in this section, can all be called in InferShape, Tiling functions.

## Data Dependency

Generally, after having input Shape, operator can infer output Shape through InferShape. However some operators during InferShape, need to depend on specific value of some input to proceed, such operators are called "data dependency operators", corresponding input is called "data dependency input". Take `Reshape` operator as example, its prototype is:

```c++
REG_OP(Reshape)
    .INPUT(x, TensorType({DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT16, DT_UINT16, DT_UINT8, DT_INT32,
        DT_INT64, DT_UINT32, DT_UINT64, DT_BOOL, DT_DOUBLE}))
    .INPUT(shape, TensorType({DT_INT32, DT_INT64}))
    .OUTPUT(y, TensorType({DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT16, DT_UINT16, DT_UINT8, DT_INT32,
        DT_INT64, DT_UINT32, DT_UINT64, DT_BOOL, DT_DOUBLE}))
    .OP_END_FACTORY_REG(Reshape)
```

`Reshape` operator adjusts `x` input's shape according to `shape` input's description (about Reshape operator function this article won't elaborate), therefore `Reshape` operator depends on `shape` input's value, take Reshape operator as example, data dependency operator during registration, needs to declare its data dependency input:

```c++
IMPL_OP(Reshape)
    .InferShape(InferShapeForReshape)
    .InputsDataDependency({1});        // Declare `ReShape` operator's 1st IR input as data dependency input, this index counts from zero
```

According to 1st input (shape input) value, Reshape operator transforms 0th input (x input) shape, and outputs to its 0th output (y output). Reshape's InferShape implementation is:

```
ge::graphStatus InferShapeForReshape(InferShapeContext *context) {
  const gert::Shape *x_shape = context->GetInputShape(0);        // Get 0th input's shape
  const gert::Tensor *shape_tensor = context->GetInputTensor(1); // Get 1st input's tensor
  gert::Shape *output_shape = context->GetOutputShape(0);
  if (x_shape == nullptr || shape_tensor == nullptr || output_shape == nullptr) {
    // Defensive programming, scenario that shouldn't appear, print error and return failure
    return ge::GRAPH_FAILED;
  }

  auto reshape_size = static_cast<int32_t>(shape_tensor->GetShapeSize());if (reshape_size < 1) {
   // Defensive programming, scenario that shouldn't appear, print error and return failure
    return ge::GRAPH_FAILED;
  }

  // According to prototype information, Reshape's shape input supports INT32 and INT64 two types, according to different types enter corresponding template function to do real shape transformation operation
  if (shape_tensor->GetDataType() == ge::DT_INT32) {
    int32_t *reshape_data = shape_tensor->GetData<int32_t>();
    return ReshapeInferShapeImpl<int32_t>(reshape_data, *x_shape, *output_shape, reshape_size);
  } else {
    int64_t *reshape_data = shape_tensor->GetData<int64_t>();
    return ReshapeInferShapeImpl<int64_t>(reshape_data, *x_shape, *output_shape, reshape_size);
  }
}
```

After getting reshape input data, finally call core `ReshapeInferShapeImpl` logic, complete reshape's InferShape logic. `ReshapeInferShapeImpl` specific business is strongly related, here won't elaborate more.

During compilation, can always call GetInputTensor to get an input's Tensor, but when its data dependency edge corresponding input is not const, tensor->GetData returns empty pointer.

> **WARNING**: For execution efficiency consideration, runtime2.0's basic API doesn't do RTTI(Runtime Type Identification), this means caller needs to guarantee correctness of getting data type by itself. For example:
>
> * Only inputs that have declared data dependency, can get their corresponding Tensor data during execution InferShape. If get Tensor data for an input that hasn't declared data dependency, then behavior is undefined.
> * When getting tensor_data from tensor (GetData\<int32_t> or GetData\<int64_t>), user needs to guarantee getting data type is correct, otherwise behavior is undefined.
>
> For convenient positioning, runtime2.0 future will do RTTI at O0 compilation option time, or enable RTTI through extra compilation option, to provide type checking capability during debug. After opening RTTI, type mismatched Get operations will lead to returning empty pointer or throwing exception.

[^1]: Instantiation: IR-defined operator is like a class definition, it defines operator prototype. Process of creating corresponding node on computation graph based on prototype, we call operator instantiation. Nodes on computation graph, we call "operator instance", inputs outputs on operator instance, we call "instantiated inputs outputs"
