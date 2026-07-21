# DataFlow接口列表

## DataFlow构图接口

**表1**  DataFlow构图接口

| 接口名称 | 简介 |
| --- | --- |
| [FlowOperator类](flowoperator-class.md) | FlowOperator类是DataFlow Graph的节点基类，继承于GE的Operator。 |
| [FlowData的构造函数和析构函数](flowdata-ctor-dtor.md) | FlowData构造函数和析构函数，构造函数会返回一个FlowData节点。 |
| [FlowNode构造函数和析构函数](flownode-ctor-dtor.md) | FlowNode构造函数和析构函数，构造函数返回一个FlowNode节点。 |
| [SetInput](SetInput.md) | 给FlowNode设置输入，表示将src_op的第src_index个输出作为FlowNode的第dst_index个输入，返回设置好输入的FlowNode节点。 |
| [AddPp](AddPp.md) | 给FlowNode添加映射的ProcessPoint，当前一个FlowNode仅能添加一个ProcessPoint，添加后会默认将FlowNode的输入输出和ProcessPoint的输入输出按顺序进行映射。 |
| [MapInput](MapInput.md) | 给FlowNode映射输入，表示将FlowNode的第node_input_index个输入给到ProcessPoint的第pp_input_index个输入，并且给ProcessPoint的该输入设置上attrs里的所有属性，返回映射好的FlowNode节点。该函数可选，不被调用时会默认按顺序映射FlowNode和ProcessPoint的输入。 |
| [MapOutput](MapOutput.md) | 给FlowNode映射输出，表示将ProcessPoint的第pp_output_index个输出给到FlowNode的第node_output_index个输出，返回映射好的FlowNode节点。 |
| [SetBalanceScatter](SetBalanceScatter.md) | 设置节点balance scatter属性，具有balance scatter属性的UDF可以使用balance options设置负载均衡输出。 |
| [SetBalanceGather](SetBalanceGather.md) | 设置节点balance gather属性，具有balance gather属性的UDF可以使用balance options设置负载均衡亲和输出。 |
| [FlowGraph构造函数和析构函数](flowgraph-ctor-dtor.md) | FlowGraph构造函数和析构函数，构造函数会返回一张空的FlowGraph图。 |
| [SetInputs](SetInputs.md) | 设置FlowGraph的输入节点，会自动根据节点的输出连接关系构建出一张FlowGraph图，并返回该图。 |
| [SetOutputs](SetOutputs.md) | 设置FlowGraph的输出节点，并返回该图。 |
| [SetOutputs（index）](setoutputs-index.md) | 设置FlowGraph中的FlowNode和FlowNode输出index的关联关系，并返回该图。常用于设置FlowNode部分输出场景，比如FlowNode1有2个输出，但是作为FlowNode2输入的时候只需要FlowNode1的一个输出，这种情况下可以设置FlowNode1的一个输出index。 |
| [SetContainsNMappingNode](SetContainsNMappingNode.md) | 设置FlowGraph是否包含n_mapping节点。 |
| [SetInputsAlignAttrs](SetInputsAlignAttrs.md) | 设置FlowGraph中的输入对齐属性。 |
| [const ge::Graph &ToGeGraph() const](const-ge-graph-togegraph-const.md) | 将FlowGraph转换到GE的Graph。 |
| [SetGraphPpBuilderAsync](SetGraphPpBuilderAsync.md) | 设置FlowGraph中的GraphPp的Builder是否异步执行。 |
| [SetExceptionCatch](SetExceptionCatch.md) | 设置用户异常捕获功能是否开启。 |
| [ProcessPoint析构函数](processpoint-dtor.md) | ProcessPoint析构函数。 |
| [GetProcessPointType](GetProcessPointType.md) | 获取ProcessPoint的类型。 |
| [GetProcessPointName](GetProcessPointName.md) | 获取ProcessPoint的名称。 |
| [GetCompileConfig](GetCompileConfig.md) | 获取ProcessPoint编译配置的文件。 |
| [Serialize（ProcessPoint类）](serialize-processpoint-class.md) | ProcessPoint的序列化方法。由ProcessPoint的子类去实现该方法的功能。 |
| [FunctionPp构造函数和析构函数](functionpp-ctor-dtor.md) | FunctionPp的构造函数和析构函数，构造函数会返回一个FunctionPp对象。 |
| [SetCompileConfig（FunctionPp类）](setcompileconfig-functionpp-class.md) | 设置FunctionPp的json配置文件名字和路径，该配置文件用于将FunctionPp和UDF进行映射。 |
| [AddInvokedClosure (添加调用的GraphPp)](addinvokedclosure-add-invoked-graph-pp.md) | 添加FunctionPp调用的GraphPp，返回添加好的FunctionPp。 |
| [AddInvokedClosure (添加调用的ProcessPoint子类)](addinvokedclosure-add-invoked-processpoint-subclass.md) | 添加FunctionPp调用的GraphPp，返回添加好的FunctionPp。 |
| [AddInvokedClosure (添加调用的FlowGraphPp)](addinvokedclosure-add-invoked-flowgraph-pp.md) | 添加FunctionPp调用的FlowGraphPp，返回添加好的FunctionPp。 |
| [SetInitParam](SetInitParam.md) | 设置FunctionPp的初始化参数，返回设置好的FunctionPp。 |
| [Serialize（FunctionPp类）](serialize-functionpp-class.md) | FunctionPp的序列化方法。 |
| [GetInvokedClosures](GetInvokedClosures.md) | 获取FunctionPp调用的GraphPp。 |
| [GraphPp构造函数和析构函数](graphpp-ctor-dtor.md) | GraphPp构造函数和析构函数，构造函数会返回一个GraphPp对象。 |
| [SetCompileConfig（GraphPp类）](setcompileconfig-graphpp-class.md) | 设置GraphPp的json配置文件路径和文件名。配置文件用于AscendGraph的描述和编译。 |
| [Serialize（GraphPp类）](serialize-graphpp-class.md) | GraphPp的序列化方法。 |
| [GetGraphBuilder（GraphPp类）](getgraphbuilder-graphpp-class.md) | 获取GraphPp中Graph的创建函数。 |
| [FlowGraphPp构造函数和析构函数](flowgraphpp-ctor-dtor.md) | FlowGraphPp构造函数和析构函数，构造函数会返回一个FlowGraphPp对象。 |
| [Serialize（FlowGraphPp类）](serialize-flowgraphpp-class.md) | FlowGraphPp的序列化方法。 |
| [GetGraphBuilder（FlowGraphPp类）](getgraphbuilder-flowgraphpp-class.md) | 获取FlowGraphPp中Graph的创建函数。 |
| [TimeBatch](TimeBatch.md) | TimeBatch功能是基于UDF为前提的。<br>正常模型每次处理一个数据，当需要一次处理一批数据时，就需要将这批数据组成一个Batch，最基本的Batch方式是将这批N个数据直接拼接，然后shape前加N，而某些场景需要将某段或者某几段时间数据组成一个batch，并且按特定的维度拼接，则可以通过使用TimeBatch功能来组Batch。 |
| [CountBatch](CountBatch.md) | CountBatch功能是指基于UDF为计算处理点将多个数据按batchSize组成batch。 |

## DataFlow运行接口

**表2**  DataFlow运行接口

| 接口名称 | 简介 |
| --- | --- |
| [FeedDataFlowGraph（feed所有输入）](feeddataflowgraph-feed-all-input.md) | 将所有数据输入到Graph图。 |
| [FeedDataFlowGraph（按索引feed输入）](feeddataflowgraph-feed-input-by-index.md) | 将数据按索引输入到Graph图。 |
| [FeedDataFlowGraph（feed所有FlowMsg）](feeddataflowgraph-feed-all-flowmsg.md) | 将数据输入到Graph图。 |
| [FeedDataFlowGraph（按索引feed FlowMsg）](feeddataflowgraph-feed-flowmsg-by-index.md) | 将数据按索引输入到Graph图。 |
| [FeedRawData](FeedRawData.md) | 将原始数据输入到Graph图。 |
| [FetchDataFlowGraph（获取所有输出数据）](fetchdataflowgraph-get-all-output-data.md) | 获取图输出数据。 |
| [FetchDataFlowGraph（按索引获取输出数据）](fetchdataflowgraph-get-output-data-by-index.md) | 按索引获取图输出数据。 |
| [FetchDataFlowGraph（获取所有输出FlowMsg）](fetchdataflowgraph-get-all-output-flowmsg.md) | 获取图输出数据。 |
| [FetchDataFlowGraph（按索引获取输出FlowMsg）](fetchdataflowgraph-get-output-flowmsg-by-index.md) | 按索引获取图输出数据。 |
| [DataFlowInfo数据类型构造函数和析构函数](dataflowinfo-datatype-ctor-dtor.md) | DataFlowInfo构造函数和析构函数。 |
| [SetUserData（DataFlowInfo数据类型）](setuserdata-dataflowinfo-datatype.md) | 设置用户信息。 |
| [GetUserData（DataFlowInfo数据类型）](getuserdata-dataflowinfo-datatype.md) | 获取用户信息。 |
| [SetStartTime（DataFlowInfo数据类型）](setstarttime-dataflowinfo-datatype.md) | 设置数据的开始时间戳。 |
| [GetStartTime（DataFlowInfo数据类型）](getstarttime-dataflowinfo-datatype.md) | 获取数据的开始时间戳。 |
| [SetEndTime（DataFlowInfo数据类型）](setendtime-dataflowinfo-datatype.md) | 设置数据的结束时间戳。 |
| [GetEndTime（DataFlowInfo数据类型）](getendtime-dataflowinfo-datatype.md) | 获取数据的结束时间戳。 |
| [SetFlowFlags（DataFlowInfo数据类型）](setflowflags-dataflowinfo-datatype.md) | 设置数据中的flags。 |
| [GetFlowFlags（DataFlowInfo数据类型）](getflowflags-dataflowinfo-datatype.md) | 获取数据中的flags。 |
| [SetTransactionId（DataFlowInfo数据类型）](settransactionid-dataflowinfo-datatype.md) | 设置DataFlow数据传输使用的事务ID。 |
| [GetTransactionId（DataFlowInfo数据类型）](gettransactionid-dataflowinfo-datatype.md) | 获取DataFlow数据传输使用的事务ID。 |
| [FlowMsg数据类型构造函数和析构函数](flowmsg-datatype-ctor-dtor.md) | FlowMsg构造函数和析构函数。 |
| [GetMsgType（FlowMsg数据类型）](getmsgtype-flowmsg-datatype.md) | 获取FlowMsg的消息类型。 |
| [SetMsgType（FlowMsg数据类型）](setmsgtype-flowmsg-datatype.md) | 设置FlowMsg的消息类型。 |
| [GetTensor（FlowMsg数据类型）](gettensor-flowmsg-datatype.md) | 获取FlowMsg中的Tensor指针。 |
| [GetRetCode（FlowMsg数据类型）](getretcode-flowmsg-datatype.md) | 获取输入FlowMsg中的错误码。 |
| [SetRetCode（FlowMsg数据类型）](setretcode-flowmsg-datatype.md) | 设置FlowMsg中的错误码。 |
| [SetStartTime（FlowMsg数据类型）](setstarttime-flowmsg-datatype.md) | 设置FlowMsg消息头中的开始时间戳。 |
| [GetStartTime（FlowMsg数据类型）](getstarttime-flowmsg-datatype.md) | 获取FlowMsg消息中的开始时间戳。 |
| [SetEndTime（FlowMsg数据类型）](setendtime-flowmsg-datatype.md) | 设置FlowMsg消息头中的结束时间戳。 |
| [GetEndTime（FlowMsg数据类型）](getendtime-flowmsg-datatype.md) | 获取FlowMsg消息中的结束时间戳。 |
| [SetFlowFlags（FlowMsg数据类型）](setflowflags-flowmsg-datatype.md) | 设置FlowMsg消息头中的flags。 |
| [GetFlowFlags（FlowMsg数据类型）](getflowflags-flowmsg-datatype.md) | 获取FlowMsg消息头中的flags。 |
| [GetTransactionId（FlowMsg数据类型）](gettransactionid-flowmsg-datatype.md) | 获取FlowMsg消息中的事务ID。 |
| [SetTransactionId（FlowMsg数据类型）](settransactionid-flowmsg-datatype.md) | 设置FlowMsg消息中的事务ID。 |
| [SetUserData（FlowMsg数据类型）](setuserdata-flowmsg-datatype.md) | 设置用户信息。 |
| [GetUserData（FlowMsg数据类型）](getuserdata-flowmsg-datatype.md) | 获取用户信息。 |
| [GetRawData（FlowMsg数据类型）](getrawdata-flowmsg-datatype.md) | 获取RawData类型的数据对应的数据指针和数据大小。 |
| [AllocTensor（FlowBufferFactory数据类型）](alloctensor-flowbufferfactory-datatype.md) | 根据shape、data type和对齐大小申请Tensor。 |
| [AllocTensorMsg（FlowBufferFactory数据类型）](alloctensormsg-flowbufferfactory-datatype.md) | 根据shape、data type和对齐大小申请FlowMsg。 |
| [AllocRawDataMsg（FlowBufferFactory数据类型）](allocrawdatamsg-flowbufferfactory-datatype.md) | 根据输入的size申请一块连续内存，用于承载raw data类型的数据。 |
| [AllocEmptyDataMsg（FlowBufferFactory数据类型）](allocemptydatamsg-flowbufferfactory-datatype.md) | 申请空数据的MsgType类型的message。 |
| [ToFlowMsg（tensor）](to-flowmsg-tensor.md) | 根据输入的Tensor转换成用于承载Tensor的FlowMsg。 |
| [ToFlowMsg（raw data）](to-flowmsg-raw-data.md) | 根据输入的raw data转换成用于承载raw data的FlowMsg。 |
