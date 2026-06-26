# 附录

## numa\_config.json配置

numa\_config.json全量字段含义如表1所示。

**表1**  字段说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|**Cluster**|-|-|-|
|cluster_nodes|Array of cluster_nodeCluster_node详细介绍请参见表2。|是|集群资源信息描述。|
|nodes_topology|nodes_topologynodes_topology详细介绍请参见表5。|否|跨节点通信拓扑。如两台AISERVER间参数面通信拓扑。|
|**node_def：集群内同种类型node的公共属性**|-|-|-|
|node_type|String|是|节点类型。示例: Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下：ATLAS900。|
|resource_type|String|是|UDF可部署的架构和CPU资源类型。取值为X86或者Aarch。|
|support_links|String|是|节点内通信方式。如[HCCS,PCIE,ROCE]。|
|item_type|String|是|节点内加速卡类型。示例: Atlas A2 训练系列产品/Atlas A2 推理系列产品场景：Ascend*xxx*B1、Ascend*xxx*B2、Ascend*xxx*B3、Ascend*xxx*B4。 Atlas A3 训练系列产品/Atlas A3 推理系列产品场景：Ascend910_*xx*|
|inter_item_memory_access_mode|String|是|Server内不同device的地址互访方式。取值范围：NUMA或者UMA。|
|h2d_bw|String|否|HOST-DEVICE间带宽如PCIE:100Gb。|
|item_topology|item_topologyitem_topology详细信息请参见表6。|否|节点内不同加速卡间互联信息。 Atlas A2 训练系列产品/Atlas A2 推理系列产品云场景下，按照1/2/4/8卡发放加速卡，故若只有1张卡则不涉及多卡拓扑。|
|**item_def：node内同种类型的加速卡的公共属性**|-|-|-|
|item_type|String|是|节点内加速卡类型。示例: Atlas A2 训练系列产品/Atlas A2 推理系列产品场景：Ascend*xxx*B1、Ascend*xxx*B2、Ascend*xxx*B3、Ascend*xxx*B4。|
|memory|String|是|整芯片总内存。Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下，请配置为[DDR:64GB]。Atlas A3 训练系列产品/Atlas A3 推理系列产品场景下，请配置为[DDR:31GB]。|
|aic_type|String|是|加速卡计算核类型和核数。如[DAVINCI_V100:32]。|
|resource_type|String|是|UDF可部署的Ascend资源类型。请配置为Ascend。|
|links_mode|String|否|芯片内互联拓扑。|
|device_list|Array of device_infodevice_info详细信息请参见表9。|否|整芯片内包含物理device信息。|

**表2**  cluster\_node说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|node_id|Integer|是|集群内节点编号，一般0作为主节点。|
|node_type|String|是|节点类型。示例: Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下：ATLAS900。|
|ipaddr|String|是|节点控制面通信的IP。如训练服务器为HOST IP。|
|port|Integer|是|节点控制面通信的端口。|
|data_panel|NetworkInfo|否|数据面通信信息。|
|memory|Integer|否|应用HOST上可用内存。|
|is_local|BOOL|否|多个Node组成集群时，此文件此节点是否是本机。|
|auth_lib_path|String|否|用来标识DataFlow需要dlopen的库文件：auth_lib_path字段由云助端CaaS写入，主从调度进程负责读取并加载so和调用相关接口。|
|deploy_res_path|String|否|模型文件存放路径。root用户默认路径：/root/runtime/deploy_res。非root用户默认路径：${home}/runtime/deploy_res。|
|item_list|Array of item_infoitem_info详细信息请参见表4。|是|云资源管理编排的执行该JOB的加速卡。|

**表3**  NetworkInfo说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|avail_ports|Array of String|否|在每个节点上该应用实例可用的数据面通信端口号范围。承载的业务如下。FlowGW异步点到点通信：HOST FlowGW和Device FlowGW通信走PCIE，不会占用HOST OS端口号；Device间FlowGW通信需要为通信分配唯一的端口号。集合通信：send/recv入图点到点、ranktbl集合通信。多个Device间FlowGW通信支持协议RDMA。多个Device间端口号默认分配范围为16666~32767，用户可自定义配置。|

**表4**  Item\_info说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|item_id|Integer|是|Node内加速卡逻辑ID。|
|device_id|Integer|是|Node内加速卡物理ID。|
|ipaddr|String|否|通信使用的IP地址或网段信息。|

**表5**  nodes\_topology说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|type|String|是|通信库根据拓扑类型来确定选择的通信算法。如star。|
|protocol|String|否|Node间通信协议类型，默认值为RDMA。单Server不需要配置该字段。示例：RDMA:200Gb。|
|topos|Array of planeplane详细信息请参考表8|是|Node间加速卡互联平面信息。|

**表6**  item\_topology说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|links_mode|String|是|节点内不同加速卡间通信方式和带宽。如HCCS:128Gb。|
|links|Array of item_pairitem_pair详细信息请参见表7。|是|节点内不同加速卡间互联拓扑。如[0, 1]。|

**表7**  item\_pair说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|item_id|Integer|是|加速卡ID。|
|pair_item_id|Integer|是|成对的加速卡ID。|

**表8**  plane说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|plane_id|Integer|是|server间通信平面是第几个plane(云提供)。|
|devices|Array of int list|是|属于该平面的加速卡列表。|

**表9**  device\_info说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|device_id|Integer|是|整芯片内物理device ID。|

<!-- npu="910b" id7 -->

**Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下的numa\_config.json示例**

numa\_config.json示例如下，全量字段含义如表1所示。

```json
{
  "cluster":[
    {
      "cluster_nodes": [
        {
          "node_id":0,
          "node_type": "ATLAS900",
          "ipaddr": "xx.xx.xx.xx",
          "port":2509,
          "memory":25165824,
          "is_local":true,
          "auth_lib_path": "/opt/huawei/config/ascend_config/dataflow/lib/libcaas_dataflow_auth.so",
          "deploy_res_path": "/home/",
          "item_list" : [
           {
             "item_id":0,
             "device_id":0,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":1,
             "device_id":1,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":2,
             "device_id":2,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":3,
             "device_id":3,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":4,
             "device_id":4,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":5,
             "device_id":5,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":6,
             "device_id":6,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":7,
             "device_id":7,
             "ipaddr":"xx.xx.xx.xx"
           }
          ]
        }
      ],
      "nodes_topology":
      {
        "protocol": "RDMA:200Gb",
        "type" : "star",
        "topos" : [
          {
            "plane_id" : 0,
            "devices" : [0, 1, 2, 3, 4, 5, 6, 7]
          },
        ]
      }
    }
  ],
  "node_def": [
    {
      "node_type": "ATLAS900" ,
      "resource_type": "Aarch",
      "support_links": "[HCCS,PCIE,ROCE]" ,
      "item_type": "AscendxxxB1",
      "inter_item_memory_access_mode": "[NUMA]",
      "h2d_bw": "PCIE:100Gb",
      "item_topology": [
        {
          "links_mode": "HCCS",
          "links": [ [0,1],[0,2],[0,3],[0,4],[0,5],[0,6],[0,7],
                   [1,2],[1,3],[1,4],[1,5],[1,6],[1,7],
                   [2,3],[2,4],[2,5],[2,6],[2,7],
                   [3,4],[3,5],[3,6],[3,7],
                   [4,5],[4,6],[4,7],
                   [5,6],[5,7],
                   [6,7]]
        }
      ]
    }
  ],
  "item_def": [
    {
      "item_type": "AscendxxxB1",
      "resource_type": "Ascend",
      "memory": "[DDR:64GB]",
      "aic_type": "[DAVINCI_V100:32]"
    }
  ]
}
```

<!-- end id7 -->

<!-- npu="A3" id8 -->

**Atlas A3 训练系列产品/Atlas A3 推理系列产品场景下的numa\_config.json示例**

numa\_config.json示例如下，全量字段含义如表1所示。

```json
{
  "cluster":[
    {
      "cluster_nodes": [
        {
          "node_id":0,
          "node_type": "ATLAS800",
          "ipaddr": "xx.xx.xx.xx",
          "port":2509,
          "memory":25165824,
          "is_local":true,
          "auth_lib_path": "/opt/huawei/config/ascend_config/dataflow/lib/libcaas_dataflow_auth.so",
          "deploy_res_path": "/home/",
          "item_list" : [
           {
             "item_id":0,
             "device_id":0,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":1,
             "device_id":1,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":2,
             "device_id":2,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":3,
             "device_id":3,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":4,
             "device_id":4,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":5,
             "device_id":5,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":6,
             "device_id":6,
             "ipaddr":"xx.xx.xx.xx"
           },
           {
             "item_id":7,
             "device_id":7,
             "ipaddr":"xx.xx.xx.xx"
           }
          ]
        }
      ],
      "nodes_topology":
      {
        "protocol": "RDMA:200Gb",
        "type" : "star",
        "topos" : [
          {
            "plane_id" : 0,
            "devices" : [0, 1, 2, 3, 4, 5, 6, 7]
          },
        ]
      }
    }
  ],
  "node_def": [
    {
      "node_type": "ATLAS900" ,
      "resource_type": "Aarch",
      "support_links": "[HCCS,PCIE,ROCE]" ,
      "item_type": "AscendxxxB1",
      "inter_item_memory_access_mode": "[NUMA]",
      "h2d_bw": "PCIE:100Gb",
      "item_topology": [
        {
          "links_mode": "HCCS",
          "links": [ [0,1],[0,2],[0,3],[0,4],[0,5],[0,6],[0,7],
                   [1,2],[1,3],[1,4],[1,5],[1,6],[1,7],
                   [2,3],[2,4],[2,5],[2,6],[2,7],
                   [3,4],[3,5],[3,6],[3,7],
                   [4,5],[4,6],[4,7],
                   [5,6],[5,7],
                   [6,7]]
        }
      ]
    }
  ],
  "item_def": [
    {
      "item_type": "Ascend910_xx",
      "resource_type": "Ascend",
      "memory": "[DDR:31GB]",
      "aic_type": "[DAVINCI_V100:32]"
    }
  ]
}
```

<!-- end id8 -->
