# --optypelist\_for\_implmode

## 功能说明

**该参数功能已停止演进，后续版本会废弃，请勿使用。**

设置optype列表中算子的实现模式，算子实现模式包括high\_precision、high\_performance两种。

## 关联参数

- 该参数需要与[--op\_select\_implmode](--op_select_implmode.md)参数配合使用，通过[--optypelist\_for\_implmode](--optypelist_for_implmode.md)参数设置的算子，统一使用[--op\_select\_implmode](--op_select_implmode.md)参数指定的实现模式，不能为列表中的每个算子设置不同的实现模式。
- 该参数配合[--op\_select\_implmode](--op_select_implmode.md)参数使用时，不能与[--op\_precision\_mode](--op_precision_mode.md)参数同时使用，若同时配置，则只有[--op\_precision\_mode](--op_precision_mode.md)参数指定的模式生效。上述参数配合运行流程请参见[图1](--op_precision_mode.md#fig1)。

## 参数取值

**参数值**：算子列表。

**参数值约束：**

- 该列表中的算子OpType必须为基于Ascend IR定义的算子的OpType，算子类型查看方法请参见[如何确定原始框架网络模型中的算子与AI处理器支持的算子的对应关系](../FAQ/operator_correspondence_guide.md)。
- 该列表中的算子使用[--op\_select\_implmode](--op_select_implmode.md)参数指定的实现模式，且仅支持指定为high\_precision、high\_performance两种模式，多个算子使用英文逗号进行分隔。
- 该参数仅对指定的算子生效，不指定的算子按照默认实现方式选择。

## 推荐配置及收益

无。

## 示例

```bash
atc --op_select_implmode=high_precision  --optypelist_for_implmode=Pooling,SoftmaxV2 ...
```

上述配置示例表示对Pooling、SoftmaxV2算子使用统一的高精度模式，未指定算子使用算子的默认实现方式。

## 依赖约束

无。
