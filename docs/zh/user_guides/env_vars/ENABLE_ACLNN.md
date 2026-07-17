# ENABLE_ACLNN

## 功能描述

**该环境变量在后续版本会废弃**。

图编译时，通过设置该环境变量，可以决定在图执行时是否调用算子注册的host执行函数来实现host执行逻辑及kernel下发。取值如下：

- true：图执行时调用算子注册的host执行函数实现host执行逻辑以及kernel下发。
- false：图执行时仍旧使用GE执行器做算子的kernel下发。
算子支持情况如下：

- 若算子**仅支持**host侧单算子执行调用接口（信息库中aclnnSupport取值配置为aclnn_only），配置该环境变量不生效，始终调用host执行函数实现host执行逻辑以及kernel下发。
- 若算子**不支持**host侧单算子执行调用接口（信息库中未配置aclnnSupport字段），则保持原有使用GE执行器做算子的kernel下发。
- 若算子两种方式都支持，则只有环境变量取值为true+信息库标记aclnnSupport（信息库中aclnnSupport取值配置为support_aclnn）+动态shape（图的shape包含-1或者-2）三个条件都满足，才调用host执行函数实现host执行逻辑以及kernel下发；否则仍旧使用GE执行器做算子的kernel下发。

## 配置示例

```bash
export ENABLE_ACLNN=true
```

## 使用约束

无

## 产品支持情况

全量芯片支持
