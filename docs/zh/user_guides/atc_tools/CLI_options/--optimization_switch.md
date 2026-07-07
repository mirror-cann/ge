# --optimization\_switch

## 产品支持情况

全量芯片支持

## 功能说明

算子编译时，图优化Pass的控制开关。

## 关联参数

该参数与精度比对中[--fusion\_switch\_file](--fusion_switch_file.md)参数的区别是：--fusion\_switch\_file仅能关闭图融合和UB融合的规则，并且需要单独配置JSON文件，而该参数适用于所有规则，通过参数就能指定融合规则，不需要再单独设置JSON文件。如果两个参数都配置，且配置了同一个融合规则，则以--optimization\_switch参数配置的为准。

## 参数取值

**参数值：**
"Passname1:on;Passname2:off"，key为Pass名称，value为on（表示开）或off（表示关）。

**参数值约束：**
可以拼接多个key-value键值对，不支持大小写模式匹配，多组配置使用英文分号分隔。可配置的融合规则请参见[融合规则列表](../references/fusion_rules_list.md)。

## 推荐配置及收益

无。

## 示例

```bash
atc --optimization_switch="Passname1:on;Passname2:off" --model=model.onnx ...
```

## 依赖约束

无。
