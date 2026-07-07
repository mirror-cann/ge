# --hccl\_sub\_comm\_config

## 产品支持情况

<!-- npu="950" id7 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id7 -->
<!-- npu="A3" id6 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id6 -->
<!-- npu="910b" id5 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id5 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id3 -->
- Atlas 推理系列产品：不支持
<!-- end id3 -->
<!-- npu="910" id2 -->
- Atlas 训练系列产品：不支持
<!-- end id2 -->
<!-- npu="IPV350" id1 -->
- IPV350：不支持
<!-- end id1 -->
<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--hccl_sub_comm_config_res.md#id1 -->

## 功能说明

设置HCCL**子通信域**配置文件路径，用于配置HCCL子通信参数。多卡分布式训练场景下使用。

## 关联参数

无

## 参数取值

**参数值：**
逻辑拓扑文件的路径和文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束：**
配置文件中的内容必须为JSON格式。

## 推荐配置及收益

无。

## 示例

将配置文件（文件名为举例为：_sub\_comm\_config.json_）上传到ATC工具所在服务器，例如上传到_$HOME/conf_，使用示例如下：

```bash
atc --model=xxx.air --framework=1 --soc_version=<soc_version> --output=$HOME/out --cluster_config=$HOME/conf/cluster_config.json  --hccl_sub_comm_config=$HOME/conf/sub_comm_config.json
```

逻辑拓扑文件示例如下：

```json
{
  "GroupList": [
    {
      "RankIds": [0,1],
      "HcclCommconfig": {
        "hcclCommName": "test_group"
      }
    }
  ]
}
```

其中：

- RankIds：rank唯一标识，请配置为整数，从0开始配置，且全局唯一，取值范围：\[0, 总Device数量-1\]。

    **建议rank\_id按照Device物理连接顺序进行排序，即将物理连接上较近的Device编排在一起，否则可能会对性能造成影响。**

    例如，若device\_ip按照物理连接从小到大设置，则rank\_id也建议按照从小到大的顺序设置。

- hcclCommName：通信组命名，供程序调用和区分。

## 使用约束

若模型算子包含子通信域参数，但在模型转换时未指定--hccl\_sub\_comm\_config参数，系统将默认依据集群的实际业务配置执行。
