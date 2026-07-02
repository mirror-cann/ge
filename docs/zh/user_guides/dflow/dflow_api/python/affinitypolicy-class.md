﻿# AffinityPolicy类

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品/Atlas A2 训练系列产品：支持

## 函数功能

亲和策略枚举定义。

## 函数原型

```python
NO_AFFINITY = (fw.AffinityPolicy.NO_AFFINITY)         # 不需要亲和
ROW_AFFINITY = (fw.AffinityPolicy.ROW_AFFINITY)   # 按行亲和，即将行相同的数据路由到相同节点。
COL_AFFINITY = (fw.AffinityPolicy.COL_AFFINITY)      # 按列亲和，即将列相同的数据路由到相同节点。
def __init__(self, inner_type):
    self.inner_type = inner_type
```

> [!NOTE]说明
>按行和按列亲和的策略只用于设置[BalanceConfig](balanceconfig-ctor.md)均衡分发。

## 参数说明

无

## 返回值

无

## 异常处理

无

## 约束说明

无
