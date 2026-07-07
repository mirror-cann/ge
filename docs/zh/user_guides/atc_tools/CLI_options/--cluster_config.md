# --cluster\_config

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
<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--cluster_config_res.md#id1 -->

## 功能说明

设置**集群**配置文件路径，用于配置目标执行逻辑设备信息以生成HCCL任务，多卡分布式训练场景下使用。

若模型中包含通信算子（当前仅支持AllGather通信算子），该参数必填。

## 关联参数

无。

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

将配置文件（文件名为举例为：_cluster\_config.json_）上传到ATC工具所在服务器，例如上传到_$HOME/conf_，使用示例如下：

```bash
atc --model=xxx.air --framework=1 --soc_version=<soc_version> --output=$HOME/out --cluster_config=$HOME/conf/cluster_config.json ...
```

逻辑拓扑文件示例如下：

```json
{
    "RankTable": {
        "status": "completed",
        "version": "1.2",
        "server_count": "2",
        "server_list": [
            {
                "server_id": "node_0",
                "device": [
                    {
                        "device_id": "0",
                        "rank_id": "0"
                    }
                ]
            },
            {
                "server_id": "node_1",
                "device": [
                    {
                        "device_id": "0",
                        "rank_id": "1"
                    }
                ]
            }
        ]
    },
    "HcclCommConfig": {
        "hcclCommName": "global name"
    }
}
```

参数说明解释如下：

- RankTable：配置的核心，定义了参与计算的节点和rank信息。
  - status：必选，rank table可用标识。
    - completed：表示rank table可用。
    - initializing：表示rank table不可用。

  - version：必选，rank table模板版本信息，请配置为：1.2。
  - server\_count：可选，参与集合通信的AI Server个数。
  - server\_list：必选，参与集合通信的AI Server列表。
    - server\_id：必选，AI Server标识，字符串类型，长度小于等于64，请确保全局唯一。配置示例：node\_0。
    - device：必选，Device列表。
      - device\_id：必选，AI处理器的物理ID，即Device在AI Server上的序列号。
      - rank\_id：必选，rank唯一标识，请配置为整数，从0开始配置，且全局唯一，取值范围：\[0, 总Device数量-1\]。

- HcclCommConfig：HCCL通信层的全局配置信息。
  - hcclCommName：必选，全局通信组的名称或标识符。

## 依赖约束

无。
