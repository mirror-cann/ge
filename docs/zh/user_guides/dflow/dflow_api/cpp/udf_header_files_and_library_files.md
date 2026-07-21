# UDF头文件和库文件说明

本文档主要描述UDF（User Defined Function）模块对外提供的接口，用户可以调用这些接口进行自定义处理函数的开发，然后通过DataFlow构图在CPU上执行该处理函数。

您可以在`${INSTALL_DIR}/include/flow_func`查看对应接口的头文件。

`${INSTALL_DIR}`请替换为CANN软件安装后文件存储路径。若安装的Ascend-cann-toolkit软件包，以root安装举例，则安装后文件存储路径为：/usr/local/Ascend/cann。

**表1**  接口分类及对应头文件

| 接口分类 | 头文件路径 | 简介 | 对应的库文件|
| --- | --- | --- |--- |
| AttrValue类 | attr_value.h | 用于获取用户设置的属性值。 | libflow_func.so |
| AscendString类 | ascend_string.h | 对String类型的封装。 | libflow_func.so |
| MetaContext类 | meta_context.h | 用于UDF上下文信息相关处理，如申请tensor和获取设置的属性等操作。 | libflow_func.so |
| FlowMsg类 | flow_msg.h | 用于处理flow func输入输出的相关操作。 | libflow_func.so |
| Tensor类 | flow_msg.h | 用于执行Tensor的相关操作。 |
| MetaFlowFunc类 | meta_flow_func.h | 该类在meta_flow_func.h中定义，用户继承该类进行自定义的单func处理函数的编写。 | libflow_func.so |
| MetaMultiFunc类 | meta_multi_func.h | 该类在meta_multi_func.h中定义，用户继承该类进行自定义的多func处理函数的编写。 | libflow_func.so |
| FlowFuncRegistrar类 | meta_multi_func.h | 该类在meta_multi_func.h中定义，是注册MetaMultiFunc的辅助模板类。 | libflow_func.so |
| MetaParams类 | meta_params.h | 该类在meta_params.h中定义，在FlowFunc的多func处理函数中使用该类获取共享的变量信息。 | libflow_func.so |
| MetaRunContext类 | meta_run_context.h | 用于执行FlowFunc的多func处理函数的上下文信息相关处理，如申请Tensor、设置输出、运行FlowModel等操作。 | libflow_func.so |
| OutOptions类 | out_options.h | 业务发布数据时，为了携带相关的option，提供了输出options的类。 | libflow_func.so |
| BalanceConfig类 | balance_config.h | 当需要均衡分发时，需要设置输出数据标识和权重矩阵相关配置信息，根据配置调度模块可以完成多实例之间的均衡分发。 | libflow_func.so |
| FlowBufferFactory类 | flow_msg.h | - | libflow_func.so |
| FlowMsgQueue类 | flow_msg_queue.h | 流式输入场景下（即flow func函数入参为队列时），用于flow func的输入队列，队列中的数据对象为[FlowMsg类](flowmsg-class.md)。 | libflow_func.so |
| 注册宏 | meta_flow_func.h<br>meta_multi_func.h | - | libflow_func.so |
| UDF日志接口 | flow_func_log.h | flow_func_log.h提供了日志接口，方便flowfunc开发中进行日志记录。 | libflow_func.so |
| 错误码 | flow_func_defines.h | - | libflow_func.so |
