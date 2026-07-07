# --enable\_scope\_fusion\_passes

## 产品支持情况

全量芯片支持

## 功能说明

指定编译时需要生效的Scope融合规则列表。

无论是内置还是用户自定义的Scope融合规则，都分为如下两类：

- 通用融合规则（General）：各网络通用的Scope融合规则；默认生效，不支持用户更改。
- 定制化融合规则（Non-General）：特定网络适用的Scope融合规则；默认不生效，用户可以通过[--enable\_scope\_fusion\_passes](--enable_scope_fusion_passes.md)指定生效的融合规则列表。

当前支持的融合规则请参见《TensorFlow Parser Scope融合规则参考》。

## 关联参数

无。

## 参数取值

**参数值：**
注册的融合规则名称。

**参数值格式：**
允许传入多个规则列表，中间使用英文逗号分隔，例如ScopePass1,ScopePass2,...。

## 推荐配置及收益

无。

## 示例

```bash
atc --enable_scope_fusion_passes=ScopePass1,ScopePass2 ...
```

## 使用约束

该参数只适用于TensorFlow网络模型。如果要查看模型转换过程中融合规则相关的日志信息，则[--log](--log.md)至少要设置为warning级别。
