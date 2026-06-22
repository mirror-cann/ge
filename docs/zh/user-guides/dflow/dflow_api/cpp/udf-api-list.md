# UDF接口列表

本文档主要描述UDF（User Defined Function）模块对外提供的接口，用户可以调用这些接口进行自定义处理函数的开发，然后通过DataFlow构图在CPU上执行该处理函数。

您可以在“$\{INSTALL\_DIR\}/include/flow\_func”查看对应接口的头文件。

$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。若安装的Ascend-cann-toolkit软件包，以root安装举例，则安装后文件存储路径为：/usr/local/Ascend/cann。

## 接口分类及对应头文件

**表1**  接口分类及对应头文件

| 接口分类 | 头文件路径 | 简介 |
| --- | --- | --- |
| AttrValue类 | attr_value.h | 用于获取用户设置的属性值。 |
| AscendString类 | ascend_string.h | 对String类型的封装。 |
| MetaContext类 | meta_context.h | 用于UDF上下文信息相关处理，如申请tensor和获取设置的属性等操作。 |
| FlowMsg类 | flow_msg.h | 用于处理flow func输入输出的相关操作。 |
| Tensor类 | flow_msg.h | 用于执行Tensor的相关操作。 |
| MetaFlowFunc类 | meta_flow_func.h | 该类在meta_flow_func.h中定义，用户继承该类进行自定义的单func处理函数的编写。 |
| MetaMultiFunc类 | meta_multi_func.h | 该类在meta_multi_func.h中定义，用户继承该类进行自定义的多func处理函数的编写。 |
| FlowFuncRegistrar类 | meta_multi_func.h | 该类在meta_multi_func.h中定义，是注册MetaMultiFunc的辅助模板类。 |
| MetaParams类 | meta_params.h | 该类在meta_params.h中定义，在FlowFunc的多func处理函数中使用该类获取共享的变量信息。 |
| MetaRunContext类 | meta_run_context.h | 用于执行FlowFunc的多func处理函数的上下文信息相关处理，如申请Tensor、设置输出、运行FlowModel等操作。 |
| OutOptions类 | out_options.h | 业务发布数据时，为了携带相关的option，提供了输出options的类。 |
| BalanceConfig类 | balance_config.h | 当需要均衡分发时，需要设置输出数据标识和权重矩阵相关配置信息，根据配置调度模块可以完成多实例之间的均衡分发。 |
| FlowBufferFactory类 | flow_msg.h | - |
| FlowMsgQueue类 | flow_msg_queue.h | 流式输入场景下（即flow func函数入参为队列时），用于flow func的输入队列，队列中的数据对象为[FlowMsg类](flowmsg-class.md)。 |
| 注册宏 | meta_flow_func.h<br>meta_multi_func.h | - |
| UDF日志接口 | flow_func_log.h | flow_func_log.h提供了日志接口，方便flowfunc开发中进行日志记录。 |
| 错误码 | flow_func_defines.h | - |

## AttrValue类

**表2**  AttrValue类接口

| 接口名称 | 简介 |
| --- | --- |
| [AttrValue构造函数和析构函数](attrvalue-ctor-dtor.md) | AttrValue构造函数和析构函数。 |
| [GetVal(AscendString &value)](getval-ascendstring-value.md) | 获取string类型的属性值。 |
| [GetVal(std::vector<AscendString> &value)](getval-std-vector-ascendstring-value.md) | 获取list string类型的属性值。 |
| [GetVal(int64_t &value)](getval-int64_t-value.md) | 获取int类型的属性值。 |
| [GetVal(std::vector<int64_t> &value)](getval-std-vector-int64_t-value.md) | 获取list int类型的属性值。 |
| [GetVal(std::vector<std::vector<int64_t >> &value)](getval-std-vector-std-vector-int64_t-value.md) | 获取list list int类型的属性值。 |
| [GetVal(float &value)](getval-float-value.md) | 获取float类型的属性值。 |
| [GetVal(std::vector<float> &value)](getval-std-vector-float-value.md) | 获取list float类型的属性值。 |
| [GetVal(bool &value)](getval-bool-value.md) | 获取bool类型的属性值。 |
| [GetVal(std::vector<bool> &value)](getval-std-vector-bool-value.md) | 获取list bool类型的属性值。 |
| [GetVal(TensorDataType &value)](getval-tensordatatype-value.md) | 获取TensorDataType类型的属性值。 |
| [GetVal(std::vector<TensorDataType> &value)](getval-std-vector-tensordatatype-value.md) | 获取list TensorDataType类型的属性值。 |

## AscendString类

**表3**  AscendString类接口

| 接口名称 | 简介 |
| --- | --- |
| [AscendString构造函数和析构函数](ascendstring-ctor-dtor.md) | AscendString构造函数和析构函数。 |
| [GetString](GetString.md) | 获取字符串地址。 |
| [关系符重载](operator-overload.md) | 对于AscendString对象大小比较的使用场景（例如map数据结构的key进行排序），通过重载以下关系符实现。 |
| [GetLength](GetLength.md) | 获取字符串的长度。 |

## MetaContext类

**表4**  MetaContext类接口

| 接口名称 | 简介 |
| --- | --- |
| [MetaContext构造函数和析构函数](metacontext-ctor-dtor.md) | MetaContext构造函数和析构函数。 |
| [AllocTensorMsg（MetaContext类）](alloctensormsg-metacontext-class.md) | 根据shape和data type申请Tensor类型的msg。该函数供[Proc](Proc.md)调用。 |
| [AllocEmptyDataMsg（MetaContext类）](allocemptydatamsg-metacontext-class.md) | 申请空数据的MsgType类型的message。该函数供[Proc](Proc.md)调用。 |
| [SetOutput（MetaContext类,tensor）](setoutput-metacontext-class-tensor.md) | 设置指定index的output的tensor。该函数供[Proc](Proc.md)调用。 |
| [GetAttr（MetaContext类，获取指针）](getattr-metacontext-class-get-ptr.md) | 根据属性名获取AttrValue类型的指针。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetAttr（MetaContext类，获取属性值）](getattr-metacontext-class-get-attr-value.md) | 根据属性名获取对应的属性值。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [RunFlowModel（MetaContext类）](runflowmodel-metacontext-class.md) | 同步执行指定的模型。该函数供[Proc](Proc.md)调用。 |
| [GetInputNum（MetaContext类）](getinputnum-metacontext-class.md) | 获取Flowfunc的输入个数。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetOutputNum（MetaContext类）](getoutputnum-metacontext-class.md) | 获取Flowfunc的输出个数。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetWorkPath（MetaContext类）](getworkpath-metacontext-class.md) | 获取Flowfunc的工作路径。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetRunningDeviceId（MetaContext类）](getrunningdeviceid-metacontext-class.md) | 获取正在运行的设备ID。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetUserData（MetaContext类）](getuserdata-metacontext-class.md) | 获取用户数据。该函数供[Proc](Proc.md)调用。 |
| [AllocTensorMsgWithAlign（MetaContext类）](alloctensormsgwithalign-metacontext-class.md) | 根据shape、data type和对齐大小申请Tensor类型的FlowMsg，与[AllocTensorMsg](alloctensormsg-metacontext-class.md)函数区别是AllocTensorMsg默认申请以64字节对齐，此函数可以指定对齐大小，方便性能调优。 |
| [RaiseException（MetaContext类）](raiseexception-metacontext-class.md) | UDF主动上报异常，该异常可以被同作用域内的其他UDF捕获。 |
| [GetException（MetaContext类）](getexception-metacontext-class.md) | UDF获取异常，如果开启了异常捕获功能，需要在UDF中Proc函数开始位置尝试捕获异常。 |

## FlowMsg类

**表5**  FlowMsg类接口

| 接口名称 | 简介 |
| --- | --- |
| [FlowMsg构造函数和析构函数](flowmsg-ctor-dtor.md) | FlowMsg构造函数和析构函数。 |
| [GetMsgType（FlowMsg类）](getmsgtype-flowmsg-class.md) | 获取FlowMsg的消息类型。 |
| [GetTensor（FlowMsg类）](gettensor-flowmsg-class.md) | 获取FlowMsg中的Tensor指针。 |
| [SetRetCode（FlowMsg类）](setretcode-flowmsg-class.md) | 设置FlowMsg消息中的错误码。 |
| [GetRetCode（FlowMsg类）](getretcode-flowmsg-class.md) | 获取输入FlowMsg消息中的错误码。 |
| [SetStartTime（FlowMsg类）](setstarttime-flowmsg-class.md) | 设置FlowMsg消息头中的开始时间戳。 |
| [GetStartTime（FlowMsg类）](getstarttime-flowmsg-class.md) | 获取FlowMsg消息中的开始时间戳。 |
| [SetEndTime（FlowMsg类）](setendtime-flowmsg-class.md) | 设置FlowMsg消息头中的结束时间戳。 |
| [GetEndTime（FlowMsg类）](getendtime-flowmsg-class.md) | 获取FlowMsg消息中的结束时间戳。 |
| [SetFlowFlags（FlowMsg类）](setflowflags-flowmsg-class.md) | 设置FlowMsg消息头中的flags。 |
| [GetFlowFlags（FlowMsg类）](getflowflags-flowmsg-class.md) | 获取FlowMsg消息头中的flags。 |
| [SetRouteLabel](SetRouteLabel.md) | 设置路由的标签。 |
| [GetTransactionId（FlowMsg类）](gettransactionid-flowmsg-class.md) | 获取FlowMsg消息中的事务ID，事务ID从1开始计数，每feed一批数据，事务ID会加一，可用于识别哪一批数据。 |
| [GetTensorList](GetTensorList.md) | 返回FlowMsg中所有的Tensor指针列表。 |
| [GetRawData（FlowMsg类）](getrawdata-flowmsg-class.md) | 获取RawData类型的数据对应的数据指针和数据大小。 |
| [SetMsgType（FlowMsg类）](setmsgtype-flowmsg-class.md) | 设置FlowMsg的消息类型。 |
| [SetTransactionId（FlowMsg类）](settransactionid-flowmsg-class.md) | 设置FlowMsg消息中的事务ID。 |

## Tensor类

**表6**  Tensor类接口

| 接口名称 | 简介 |
| --- | --- |
| [Tensor构造函数和析构函数](tensor-ctor-dtor.md) | Tensor构造函数和析构函数。 |
| [GetShape](GetShape.md) | 获取Tensor的Shape。 |
| [GetDataType](GetDataType.md) | 获取Tensor中的数据类型。 |
| [GetData](GetData.md) | 获取Tensor中的数据。 |
| [GetDataSize](GetDataSize.md) | 获取Tensor中的数据大小。 |
| [GetElementCnt](GetElementCnt.md) | 获取Tensor中的元素的个数。 |
| [GetDataBufferSize](GetDataBufferSize.md) | 获取Tensor中的对齐后的数据大小。 |
| [Reshape](Reshape.md) | 对Tensor进行Reshape操作，不改变Tensor的内容。 |

## MetaFlowFunc类

**表7**  MetaFlowFunc类接口

| 接口名称 | 简介 |
| --- | --- |
| [MetaFlowFunc构造函数和析构函数](metaflowfunc-ctor-dtor.md) | 用户继承该类进行自定义的单func处理函数的编写。在析构函数中，执行释放相关资源操作。 |
| [SetContext](SetContext.md) | 设置flow func的上下文信息。 |
| [Init（MetaFlowFunc类）](init-metaflowfunc-class.md) | 用户自定义flow func的初始化函数。 |
| [Proc](Proc.md) | 用户自定义flow func的处理函数。 |
| [RegisterFlowFunc](RegisterFlowFunc.md) | 注册flow func。<br>不建议直接使用该函数，建议使用[MetaFlowFunc注册函数宏](metaflowfunc-register-func-macro.md)来注册flow func。 |
| [ResetFlowFuncState（MetaFlowFunc类）](resetflowfuncstate-metaflowfunc-class.md) | 在故障恢复场景下，快速重置FlowFunc为初始化状态。 |
| [其他](others.md) | REGISTER_FLOW_FUNC_INNER(name, ctr, clazz)和REGISTER_FLOW_FUNC_IMPL(name, ctr, clazz)是[MetaFlowFunc注册函数宏](metaflowfunc-register-func-macro.md)的实现，不建议用户直接调用。 |

## MetaMultiFunc类

**表8**  MetaMultiFunc类接口

| 接口名称 | 简介 |
| --- | --- |
| [MetaMultiFunc构造函数和析构函数](metamultifunc-ctor-dtor.md) | 用户继承该类进行自定义的多func处理函数的编写。在析构函数中，执行释放相关资源操作。 |
| [Init（MetaMultiFunc类）](init-metamultifunc-class.md) | 用户自定义flow func的初始化函数。 |
| [多func处理函数](multi-func-handler.md) | 用户自定义多flow func的处理函数。 |
| [RegisterMultiFunc](RegisterMultiFunc.md) | 注册多flow func。<br>不建议直接使用该函数，建议使用[MetaMultiFunc注册函数宏](metamultifunc-register-func-macro.md)来注册flow func。 |
| [ResetFlowFuncState（MetaMultiFunc类）](resetflowfuncstate-metamultifunc-class.md) | 在故障恢复场景下，快速重置FlowFunc为初始化状态。 |

## FlowFuncRegistrar类

**表9**  FlowFuncRegistrar类接口

| 接口名称 | 简介 |
| --- | --- |
| [RegProcFunc](RegProcFunc.md) | 注册多flow func处理函数，结合[MetaMultiFunc注册函数宏](metamultifunc-register-func-macro.md)来注册flow func。 |
| [CreateMultiFunc](CreateMultiFunc.md) | 创建多func处理对象和处理函数，框架内部使用，用户不直接使用。 |

## MetaParams类

**表10**  MetaParams类接口

| 接口名称 | 简介 |
| --- | --- |
| [MetaParams构造函数和析构函数](metaparams-ctor-dtor.md) | MetaParams构造函数和析构函数。 |
| [GetName](GetName.md) | 获取Flowfunc的实例名。 |
| [GetAttr（MetaParams类，获取指针）](getattr-metaparams-class-get-ptr.md) | 根据属性名获取AttrValue类型的指针。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetAttr（MetaParams类，获取属性值）](getattr-metaparams-class-get-attr-value.md) | 根据属性名获取对应的属性值。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetInputNum（MetaParams类）](getinputnum-metaparams-class.md) | 获取Flowfunc的输入个数。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetOutputNum（MetaParams类）](getoutputnum-metaparams-class.md) | 获取Flowfunc的输出个数。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetWorkPath（MetaParams类）](getworkpath-metaparams-class.md) | 获取Flowfunc的工作路径。该函数供[Init（MetaFlowFunc类）](init-metaflowfunc-class.md)调用。 |
| [GetRunningDeviceId（MetaParams类）](getrunningdeviceid-metaparams-class.md) | 获取正在运行的设备ID。 |

## MetaRunContext类

**表11**  MetaRunContext类接口

| 接口名称 | 简介 |
| --- | --- |
| [MetaRunContext构造函数和析构函数](metaruncontext-ctor-dtor.md) | MetaRunContext构造函数和析构函数。 |
| [AllocTensorMsg（MetaRunContext类）](alloctensormsg-metaruncontext-class.md) | 根据shape和data type申请Tensor类型的msg。该函数供[Proc](Proc.md)调用。 |
| [SetOutput（MetaRunContext类,tensor）](setoutput-metaruncontext-class-tensor.md) | 设置指定index的output的tensor。该函数供[Proc](Proc.md)调用。 |
| [RunFlowModel（MetaRunContext类）](runflowmodel-metaruncontext-class.md) | 同步执行指定的模型。该函数供[Proc](Proc.md)调用。 |
| [AllocEmptyDataMsg（MetaRunContext类）](allocemptydatamsg-metaruncontext-class.md) | 申请空数据的MsgType类型的message。 |
| [GetUserData（MetaRunContext类）](getuserdata-metaruncontext-class.md) | 获取用户数据。该函数供[Proc](Proc.md)调用。 |
| [SetOutput（MetaRunContext类,输出）](setoutput-metaruncontext-class-output.md) | 设置指定index和options的输出，该函数供func函数调用。 |
| [SetMultiOutputs](SetMultiOutputs.md) | 批量设置指定index和options的输出，该函数供func函数调用。 |
| [AllocTensorMsgWithAlign（MetaRunContext类）](alloctensormsgwithalign-metaruncontext-class.md) | 根据shape、data type和对齐大小申请Tensor类型的FlowMsg，与[AllocTensorMsg](alloctensormsg-metacontext-class.md)函数区别是AllocTensorMsg默认申请以64字节对齐，此函数可以指定对齐大小，方便性能调优。 |
| [AllocTensorListMsg](AllocTensorListMsg.md) | 根据输入的dtype shapes数组分配一块连续内存，用于承载Tensor数组。 |
| [RaiseException（MetaRunContext类）](raiseexception-metaruncontext-class.md) | UDF主动上报异常，该异常可以被同作用域内的其他UDF捕获。 |
| [GetException（MetaRunContext类）](getexception-metaruncontext-class.md) | UDF获取异常，如果开启了异常捕获功能，需要在UDF中Proc函数开始位置尝试捕获异常。 |
| [AllocRawDataMsg（MetaRunContext类）](allocrawdatamsg-metaruncontext-class.md) | 根据输入的size申请一块连续内存，用于承载RawData类型的数据。 |
| [ToFlowMsg](ToFlowMsg.md) | 根据输入的Tensor转换成用于承载Tensor的FlowMsg。 |

## OutOptions类

**表12**  OutOptions类接口

| 接口名称 | 简介 |
| --- | --- |
| [OutOptions构造函数和析构函数](outoptions-ctor-dtor.md) | OutOptions的构造和析构函数。 |
| [MutableBalanceConfig](MutableBalanceConfig.md) | 获取或创建BalanceConfig。 |
| [GetBalanceConfig](GetBalanceConfig.md) | 获取BalanceConfig。 |

## BalanceConfig类

**表13**  BalanceConfig类接口

| 接口名称 | 简介 |
| --- | --- |
| [BalanceConfig构造函数和析构函数](balanceconfig-ctor-dtor.md) | BalanceConfig的构造和析构函数。 |
| [SetAffinityPolicy](SetAffinityPolicy.md) | 设置均衡分发亲和性。 |
| [GetAffinityPolicy](GetAffinityPolicy.md) | 获取亲和性。 |
| [SetBalanceWeight](SetBalanceWeight.md) | 设置均衡分发权重信息。 |
| [GetBalanceWeight](GetBalanceWeight.md) | 获取均衡分发权重信息。 |
| [SetDataPos](SetDataPos.md) | 设置输出数据对应权重矩阵中的位置。 |
| [GetDataPos](GetDataPos.md) | 获取输出数据对应权重矩阵中的位置。 |

## FlowBufferFactory类

**表14**  FlowBufferFactory类接口

| 接口名称 | 简介 |
| --- | --- |
| [AllocTensor（FlowBufferFactory类）](alloctensor-flowbufferfactory-class.md) | 根据shape、data type和对齐大小申请Tensor，默认申请以64字节对齐，可以指定对齐大小，方便性能调优。 |

## FlowMsgQueue类

**表15**  FlowMsgQueue类接口

| 接口名称 | 简介 |
| --- | --- |
| [FlowMsgQueue构造函数和析构函数](flowmsgqueue-ctor-dtor.md) | FlowMsgQueue的构造和析构函数。 |
| [Dequeue](Dequeue.md) | 设置均衡分发亲和性。 |
| [Depth](Depth.md) | 获取队列的深度，即获取队列可容纳元素的最大个数。 |
| [Size](Size.md) | 获取队列中当前元素的个数。 |

## 注册宏

**表16**  注册宏

| 接口名称 | 简介 |
| --- | --- |
| [MetaFlowFunc注册函数宏](metaflowfunc-register-func-macro.md) | 注册MetaFlowFunc的实现类。 |
| [MetaMultiFunc注册函数宏](metamultifunc-register-func-macro.md) | 注册MetaMultiFunc的实现类。 |

## UDF日志接口

**表17**  UDF日志接口

| 接口名称 | 简介 |
| --- | --- |
| [FlowFuncLogger构造函数和析构函数](flowfunclogger-ctor-dtor.md) | FlowFuncLogger构造函数和析构函数。 |
| [GetLogger](GetLogger.md) | 获取日志实现类。 |
| [GetLogExtHeader](GetLogExtHeader.md) | 获取日志扩展头信息。 |
| [IsLogEnable](IsLogEnable.md) | 查询对应级别和类型的日志是否开启。 |
| [Error](Error.md) | 记录ERROR级别日志。 |
| [Warn](Warn.md) | 记录Warn级别日志。 |
| [Info](Info.md) | 记录Info级别日志。 |
| [Debug](Debug.md) | 记录Debug级别日志。 |
| [运行日志Error级别日志宏](run-log-error-level-log-macro.md) | 运行日志Error级别日志宏。 |
| [运行日志Info级别日志宏](run-log-info-level-log-macro.md) | 运行日志Info级别日志宏。 |
| [调试日志Error级别日志宏](debug-log-error-level-log-macro.md) | 调试日志Error级别日志宏。 |
| [调试日志Warn级别日志宏](debug-log-warn-level-log-macro.md) | 调试日志Warn级别日志宏。 |
| [调试日志Info级别日志宏](debug-log-info-level-log-macro.md) | 调试日志Info级别日志宏。 |
| [调试日志Debug级别日志宏](debug-log-debug-level-log-macro.md) | 调试日志Debug级别日志宏。 |

## 错误码

**表18**  错误码

| 错误码模块 | 简介 |
| --- | --- |
| [flowfunc](udf-error-code.md#flowfunc) | 提供了flowfunc的错误码供用户使用，主要用于对异常逻辑的判断处理。 |
| [AICPU](udf-error-code.md#aicpu) | AICPU在执行模型的过程中，有可能向用户上报的错误码。 |
