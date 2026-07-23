# DataFlow头文件和库文件说明

您可以在`${INSTALL_DIR}/include/flow_graph`和`${INSTALL_DIR}/include/ge`路径下查看对应接口的头文件。

`${INSTALL_DIR}`请替换为CANN软件安装后文件存储路径。若安装的Ascend-cann-toolkit软件包，以root安装举例，则安装后文件存储路径为：/usr/local/Ascend/cann。

| 接口分类 | 头文件路径 | 简介 | 对应的库文件 |
| --- | --- | --- | --- |
| FlowOperator类 | flow_graph.h | FlowOperator类是DataFlow Graph的节点基类，继承于GE的Operator。不支持在外部单独构造使用。 | NA |
| FlowData类 | flow_graph.h | 继承于FlowOperator类，为DataFlow Graph中的数据节点，每个FlowData对应一个输入。 | libflow_graph.so |
| FlowNode类 | flow_graph.h | 继承于FlowOperator类，DataFlow Graph中的计算节点。 | libflow_graph.so |
| FlowGraph类 | flow_graph.h | DataFlow的graph，由输入节点FlowData和计算节点FlowNode构成。 | libflow_graph.so |
| ProcessPoint类 | process_point.h | ProcessPoint是一个虚基类，无法实例化对象。 | NA |
| FunctionPp类 | process_point.h | 继承于[ProcessPoint类](processpoint-class.md)，用来表示Function的计算处理点。 | libflow_graph.so |
| GraphPp类 | process_point.h | 继承自[ProcessPoint类](processpoint-class.md)，用来表示Graph的计算处理点。 | libflow_graph.so |
| DataFlowInputAttr结构体 | flow_attr.h | 定义timeBatch和countBatch两种功能实现UDF组batch能力。 | libflow_graph.so |
| Graph运行接口 | ge_api.h | 用于将数据输入到DataFlow图和获取DataFlow模型执行结果。 | libge_runner.so、libdavinci_executor.so和libgraph_base.so |
| 数据类型 | ge_data_flow_api.h | 支持用户设置和获取DataFlowInfo中的成员变量。<br> 说明：如果单点编译DataFlowInfo数据类型，建议编译选项增加-Wl,--no-as-needed，确保依赖的so符号在编译时被完整加载。 | libdavinci_executor.so和libge_runner.so |

> [!NOTE]说明
>头文件中出现的char\_t类型是char类型的别名。
