# --input\_fp16\_nodes

## 产品支持情况

全量芯片支持

## 功能说明

指定输入数据类型为float16的输入节点名称。

## 关联参数

- 若配置了该参数，则不能对同一个输入节点同时使用[--insert\_op\_conf](--insert_op_conf.md)参数。
- 当[--framework](--framework.md)取值为1，且为MindSpore框架网络模型时，设置本参数无效，但模型转换成功。

## 参数取值

**参数值：**
数据类型为float16的输入节点名称。

**参数值约束：**
指定的节点有多个时，必须放在双引号中，节点中间使用**英文分号**分隔。

## 推荐配置及收益

无。

## 示例

```bash
atc --input_fp16_nodes="node_name1;node_name2" ...
```

## 依赖约束

无。
