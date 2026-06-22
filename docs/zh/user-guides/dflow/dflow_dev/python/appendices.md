# 附录

## DataFlow框架介绍

**DataFlow框架结构**

**图1**  DataFlow框架结构  
![](figures/DataFlow框架结构.png "DataFlow框架结构")

DataFlow结构可以分为3层，各层介绍如下。

- DataFlow API：负责异构计算流表达，App开发人员使用DataFlow API表达业务的数据流，生成基于Graph IR的计算图。数据流表达详细描述请参见“数据流表达”。
- 异构并行编译器：负责异构计算图编译，将计算图基于异构计算单元的组成关系和连接拓扑进行图切分、图优化和图部署，生成各个异构计算单元的执行Bin。
- 异构Runtime：根据异构计算图编译输出的结果，将执行Bin调度到对应的异构执行单元上执行。异构Runtime详细描述请参见“异构Runtime”。

**数据流表达**

数据流图表达的规则和约束如下。

- FlowGraph是DAG（Directed Acyclic Graph）图，数据流有向且不允许有成环表达。
- 不支持无输入或无输出的FlowGraph。
- 节点间流转的数据可能存在多节点共享，要保证不可变，因此不允许计算节点修改输入数据，必要时需要做拷贝处理。
- 数据流支持1对多，表示复制相同数据后分发给多个Node；但同一个节点的多个输出发给同一个输入。
      ![](figures/260109170920368.png)

- 支持表达调用GraphProcessPoint执行，实现控制流表达如循环迭代，分支控制逻辑如下。
      ![](figures/250530151714622.png)

**异构Runtime**

异构Runtime针对CPU、NPU异构节点提供不同的执行器，包括如下。

- CPU图执行器：部署在CPU节点，用于执行计算图。
- NPU图执行器：部署于NPU，用于执行计算图。
- UDF执行器：自定义Func执行引擎，可灵活部署于CPU节点或使用NPU上的AICPU。

各执行器以独立进程方式部署，每个执行进程间采用如下数据通信方式交互数据：在同一节点内采用共享内存、共享队列；跨节点使用RDMA/UB高性能通信。

执行器间数据流的生产、消费关系/1对多分发关系/跨节点数据通信统一由FlowGW（数据流网关）代理并转发。

![](figures/260109171126678.png)

## Mbuf多内存

**功能介绍**

Mbuf是一种共享内存，包含了MbufHead和MbufData两部分数据，如表1所示。

**表1**  Mbuf介绍

|数据类型|描述|
|--|--|
|MbufHead|即数据头，包含了以下几类数据：基础的数据内容：比如DataType/DataSize/Shape等信息。数据传递涉及的关键信息：retCode/routeLabel等框架相关信息。其他用户可配置字段：starttime/endtime/userdata等。|
|MbufData|一段Mbuf上的内存，存储了实际上有效的数据内容。|

在DataFlow的内存分配场景中，Mbuf是驱动对于内存管理的统一机制，所有的FlowMsg都会在Mbuf池中进行生成并使用。每个FlowMsg都会申请一段Mbuf内存：

```sh
sizeMbuf  = 
       sizeof(MbufData) // FlowMsg实际包含的内存大小
     + sizeof(MbufHead) // MbufHead大小,实际为1024B
```

> [!NOTE]说明
>FlowMsg是FlowGraph上节点间传递信息的载体，用于处理FlowFunc输入输出的相关操作。FlowMsg使用共享内存Mbuf来保存信息。

**背景信息**

用户存在大量UDF进程，且每个UDF进程都可能申请大量小内存的场景下，建议使用Mbuf多内存池。如下举例说明。

假设Mbuf内存池总大小是10GB，不使用Mbuf多内存池时，由于内存碎片的存在，无法申请连续的大块内存。详细信息如下。

![](figures/250311111739664.png)

1. 在time1的时间点，申请了多个大块内存（3GB）和多个小块内存（2MB）。
2. 在time2的时间点上，大内存不再使用，释放给内存池变为空闲状态。此时，从内存池整体状况来看，整体内存使用情况是：3GB（空闲）+2MB（已申请）+3GB（空闲）+2MB（已申请）+3GB（空闲）+2MB（已申请），可用内存为9GB+。
3. 在time3的时间点上，虽然可用内存为9GB+，但是由于内存碎片（2MB内存块）的存在，所以申请5GB的连续内存失败。

**使用示例**

针对上述场景，需要额外新增内存池配置，用以处理内存碎片问题。主要方法是，将Mbuf设置为可配置多内存池系统，小内存放入专用小内存池中。

Mbuf多内存池示例图如下。

![](figures/250311111739664-0.png)

1. 在time1的时间点，申请了多个大块内存（3GB）和多个小块内存（2MB）。
2. 在time2的时间点上，大内存不再使用，释放给内存池变为空闲状态。此时，从内存池整体状况来看，整体内存使用情况是：3GB（空闲）+3GB（空闲）+3GB（空闲）+2MB（已申请）+2MB（已申请），内存池1中可用内存为9GB+，内存池2中可用内存为1G+。

3. 在time3的时间点上，内存池1中可用连续内存为9GB+，申请5GB的内存成功。

该方案是通过配置“ge.flowGraphMemMaxSize”实现的，配置示例如表2中的示例二。

**表2**  配置示例

|示例名|示例描述|示例代码|
|--|--|--|
|示例一|申请一个内存池，大小为10GB，申请内存大小不限制。|{"ge.flowGraphMemMaxSize", "10737418240"}|
|示例二|申请两个内存池。<br>- 内存池1大小为10GB，申请内存大小不限制。<br>- 内存池2大小为2GB，最大可以申请2MB。<br>申请内存<=2MB时会优先在内存池2中申请，申请内存>2MB时只能在内存池1中申请。|{"ge.flowGraphMemMaxSize", "10737418240,2147483648:2097152"}|
|示例三|申请三个内存池。<br>- 内存池1大小为10GB，申请内存大小不限制。<br>- 内存池2大小为2GB，最大可以申请20MB。<br>- 内存池3大小为1GB，最大可以申请2MB。<br>申请内存时会根据申请内存大小优先匹配对应的内存池，比如申请内存<=2MB时会优先在内存池3中申请，2MB<申请内存<=20MB的优先在内存池2中申请，申请内存>20MB的只能在内存池3中申请。|{"ge.flowGraphMemMaxSize", "10737418240,2147483648:20971520,1073741824:2097152"}|

> [!NOTE]说明
><br>ge.flowGraphMemMaxSize具体含义如下。
><br>FlowGraph场景下，网络可使用的内存配置，可根据网络大小指定。单位：Byte，可以支持多个内存池配置，最多支持128个内存池，配置格式为"内存池1大小:申请内存限制,内存池2大小:内存申请限制,...,内存池n大小:内存申请限制",如果不设置，默认单个内存池，大小10GB，申请内存大小不限制。
><br>内存池大小取值范围为\[1024, 274877906944\]，单位为Byte，所有内存池大小加起来不能超过256GB，实际使用时受硬件限制。
><br>申请内存限制取值范围为0或者\[1024,2147483648\]，单位为Byte，为可选配置，默认为0，表示不限制。

## numa\_config.json配置

numa\_config.json全量字段含义如表1所示。

**表1**  字段说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|**Cluster**|-|-|-|
|cluster_nodes|Array of cluster_nodeCluster_node详细介绍请参见表2。|是|集群资源信息描述。|
|nodes_topology|nodes_topologynodes_topology详细介绍请参见表5。|否|跨节点通信拓扑。如两台AISERVER间参数面通信拓扑。|
|**node_def：集群内同种类型node的公共属性**|-|-|-|
|node_type|String|是|节点类型。示例如：Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下：ATLAS900。|
|resource_type|String|是|UDF可部署的架构和CPU资源类型。取值为X86或者Aarch。|
|support_links|String|是|节点内通信方式。如[HCCS,PCIE,ROCE]。|
|item_type|String|是|节点内加速卡类型。示例：Atlas A2 训练系列产品/Atlas A2 推理系列产品场景：Ascend*xxx*B1、Ascend*xxx*B2、Ascend*xxx*B3、Ascend*xxx*B4。Atlas A3 训练系列产品/Atlas A3 推理系列产品场景：Ascend910_*xx*|
|inter_item_memory_access_mode|String|是|Server内不同device的地址互访方式。取值范围：NUMA或者UMA。|
|h2d_bw|String|否|HOST-DEVICE间带宽如PCIE:100Gb。|
|item_topology|item_topologyitem_topology详细信息请参见表6。|否|节点内不同加速卡间互联信息。Atlas A2 训练系列产品/Atlas A2 推理系列产品云场景下，按照1/2/4/8卡发放加速卡，故若只有1张卡则不涉及多卡拓扑。|
|**item_def：node内同种类型的加速卡的公共属性**|-|-|-|
|item_type|String|是|节点内加速卡类型。示例：Atlas A2 训练系列产品/Atlas A2 推理系列产品场景：Ascend*xxx*B1、Ascend*xxx*B2、Ascend*xxx*B3、Ascend*xxx*B4。|
|memory|String|是|整芯片总内存。 Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下，请配置为[DDR:64GB]。Atlas A3 训练系列产品/Atlas A3 推理系列产品场景下，请配置为[DDR:31GB]。|
|aic_type|String|是|加速卡计算核类型和核数。如[DAVINCI_V100:32]。|
|resource_type|String|是|UDF可部署的Ascend资源类型。请配置为Ascend。|
|links_mode|String|否|芯片内互联拓扑。|
|device_list|Array of device_infodevice_info详细信息请参见表9。|否|整芯片内包含物理device信息。|

**表2**  cluster\_node说明

|**名称**|**类型**|**是否必选**|**描述**|
|--|--|--|--|
|node_id|Integer|是|集群内节点编号，一般0作为主节点。|
|node_type|String|是|节点类型。示例：Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下：ATLAS900。|
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

<!-- npu="910b" id9 -->

**Atlas A2 训练系列产品/Atlas A2 推理系列产品场景下的numa\_config.json示例**

numa\_config.json示例如下，全量字段含义如表1所示。

```python
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

<!-- end id9 -->

<!-- npu="A3" id10 -->

**Atlas A3 训练系列产品/Atlas A3 推理系列产品场景下的numa\_config.json示例**

numa\_config.json示例如下，全量字段含义如表1所示。

```python
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

<!-- end id10 -->

## UDF Python工程创建工具

该工具的作用是创建一个UDF Python工程模板，包括对应的文件夹和文件，用户可以基于该模板进行功能的开发，减少了手工创建的工作量。

该工具存放路径：\$\{install\_path\}/cann/python/site-packages/dataflow/tools/create\_func\_ws.py，其中$\{install\_path\}为CANN软件的安装目录，默认是"/usr/local/Ascend"。

使用方法如下。

1. 设置环境变量。

    安装CANN软件后，使用CANN运行用户进行编译、运行时，需要以CANN运行用户登录环境，执行**source \$*\{INSTALL\_DIR\}*/set\_env.sh**命令设置环境变量。$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

2. 创建UDF Python工程。

    如下以创建工作目录为“test\_sub”的工程为例，说明如何创建。

    在任意路径下执行如下命令。

    ```shell
    python3.11 -m dataflow.tools.create_func_ws -f sub:i0:i1:o0 -w ./test_sub -c Sub
    ```

    在当前目录下，生成如下“test\_sub”文件夹，目录结构如下。

    ```sh
    ├── CMakeLists.txt  // CMakeLists编译配置文件，一般不需要用户改动。如果打包需要特殊处理可以修改，比如打包时增加配置文件发布。
    ├── func_sub.json // UDF配置文件，用于指定UDF的输入，输出等，一般不需要用户改动，如果生成工作workspace目录和实际运行时不一致，需要修改文件中workspace参数为实际运行时路径。
    ├── src_cpp
    │   └── func_sub.cpp  // 完成UDF注册，以及UDF的C++调用Python的逻辑，不需要用户改动。
    └── src_python
        └── func_sub.py  // 用户的实现函数，需要用户自行编写，当前目录下可以增加多个Python文件，打包时会一并打包到运行环境。
    ```

    命令中各参数含义如下。

    - -m：必选参数，Python命令行通用参数，作为脚本运行库模块，该工具中固定为dataflow.tools.create\_func\_ws。
    - -f：必选参数，输入函数信息、输入和输出索引，格式为“函数名:ix:ix:ox:ox”，i开头表示输入，后面跟输入索引，o开头表示输出，后面跟输出索引，多个函数的话中间以半角逗号分隔。例如，sub:i0:i1:o0，该示例中，函数名为sub（会直接作为Python函数名，建议采用小写加下划线风格，同时生成的cpp代码会按照大驼峰拼接“Proc”作为cpp函数），两个输入：i0和i1，一个输出o0。
    - -w：可选参数，工作目录。例如，./test\_sub，该示例中，指定工作目录是当前目录下的“test\_sub”，默认值为当前目录“./”，注意：生成的文件会直接覆盖工作空间中重名的文件。
    - -c：可选参数，函数类名。例如，Sub\(会直接作为Python和c++类名，建议使用大驼峰风格\)，为空时会使用-f参数中的函数名拼接生成。

该工具的更多使用参数请通过“-h”进行查看。
