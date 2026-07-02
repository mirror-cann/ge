# get\_transaction\_id

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品/Atlas A2 训练系列产品：支持

## 函数功能

获取FlowMsg消息中的事务ID，事务ID从1开始计数，每feed一批数据，事务ID会加一，可用于识别哪一批数据。

## 函数原型

```python
get_transaction_id(self) -> int
```

## 参数说明

无

## 返回值

事务ID。

## 异常处理

无

## 约束说明

无
