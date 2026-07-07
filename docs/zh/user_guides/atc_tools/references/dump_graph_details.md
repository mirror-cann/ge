# dump图详细信息

模型转换前，通过设置如下环境变量，dump出编译过程中的模型图，用户可以通过查看dump图观察模型的变化：

```bash
export DUMP_GE_GRAPH=1      #控制dump图的内容多少
export DUMP_GRAPH_LEVEL=1   #控制dump图的个数
```

在执行atc命令的当前路径会生成如下文件，关于环境变量的详细介绍请参见[2](../overview/environment_setup.md)。

- ge\_onnx\*.pbtxt：基于ONNX的模型描述结构，可以使用Netron等可视化软件打开。
- ge\_proto\*.txt：protobuf格式存储的文本文件，该文件可以转成JSON格式文件方便用户定位问题。该文件与ge\_onnx\*.pbtxt一般成对出现，但是ge\_proto\*.txt比ge\_onnx\*.pbtxt文件会多string类型的属性信息，因此ge\_proto\*.txt显示得更完整，用户选择其中一种文件打开。

    由于ge\_proto\*.txt文件结构相比ge\_onnx\*.pbtxt已经做了文件大小的优化，因此DUMP\_GE\_GRAPH环境变量设置为2或3，对ge\_proto\*.txt文件效果相同，都显示为不含有权重等数据的基本版dump。

- ge\_readable\*.txt：（可选）类似Dynamo fx图风格的高可读性文本文件。该类型的文件只在设置了DUMP\_GRAPH\_FORMAT环境变量，且取值中包括“readable”才会生成。

上述每个文件对应模型编译过程中的一个步骤，比如以ge\_onnx\_xxxx\_PreRunBegin.pbtxt开始，以ge\_onnx\_xxxx\_PreRunAfterBuild.pbtxt结尾。每个文件中包括完成该步骤所涉及的所有算子，关于dump图每个阶段的子图详细作用请参见下表（每个模型生成的dump子图可能不一致，但是主流程基本一致）。

**表 1**  dump图详细信息说明

| 序号 | 子图名称 | 涉及组件 | 所处阶段描述 | 是否白名单 |
| --- | --- | --- | --- | --- |
| 1 | ge_proto_xxxx_GraphPreRunBegin.txt | GE | Graph编译前的图 | - |
| 2 | ge_proto_xxxx_AfterFlowGraphPartition.txt | GE | Flow切分后的图（flow切分是应用于DataFlow中的切分方式） | - |
| 3 | ge_proto_xxxx_AfterParallelPartitioner.txt | GE | pipeline并行切分后的图（这里的pipeline指后端推理场景的PP） | - |
| 4 | ge_proto_xxxx_PreRunBegin.txt | GE | 原始图结构 | 白名单 |
| 5 | ge_proto_xxxx_RunCustomPassBeforeInfershape.txt | GE | 在InferShape之前用户自定义Pass的出口图 | 白名单 |
| 6 | ge_proto_xxxx_PreRunAfterInitPreparation.txt | FE | 经历了图准备阶段所有初始化处理之后的图结构 | - |
| 7 | ge_proto_xxxx_PreRunAfterHandleSummaryOp.txt | GE | 对Summary节点做处理之后的图结构 | - |
| 8 | ge_proto_xxxx_PrepareAfterCheckAndUpdateInput.txt | GE | 校验并更新图输入数据处理之后的图结构 | - |
| 9 | ge_proto_xxxx_PrepareAfterGraphEquivalentTransformation.txt | GE | 将For循环图结构同等替换成While循环图结构处理之后的图结构 | - |
| 10 | ge_proto_xxxx_PrepareAfterProcessOutput.txt | GE | 对图数据进行相关处理之后的图结构 | - |
| 11 | ge_proto_xxxx_PrepareAfterOptimizeAfterGraphNormalization.txt | GE | 图标准化后图优化操作出口图 | - |
| 12 | ge_proto_xxxx_PrepareAfterInsertAipp.txt | GE | 在配置了AIPP参数下，对图进行AIPP相关处理之后的图结构 | - |
| 13 | ge_proto_xxxx_PrepareAfterProcessAippNodesDataFormat.txt | GE | AIPP节点格式刷新出口图 | - |
| 14 | ge_proto_xxxx_PreRunAfterNormalizeGraph.txt | GE | 图标准化出口图 | 白名单 |
| 15 | ge_proto_xxxx_PreRunAfterOptimizeGraphInit.txt | GE | 图优化初始化出口图 | - |
| 16 | ge_proto_xxxx_OptimizeGraph_TagNoConstFoldingAfter.txt | FE | 量化场景使用，FE会给算子打上不做常量折叠标签，GE在执行常量折叠时会判断此标签，如果存在，则不执行常量折叠 | - |
| 17 | ge_proto_xxxx_HcclAfterOptimizeGraphPrepare.txt | HCCL | HCCL图准备阶段优化后的图 | - |
| 18 | ge_proto_xxxx_PreRunAfterOptimizeGraphPrepare.txt | GE | 经过各算子信息库原图准备处理（OptimizeGraphPrepare接口调用）之后的图结构 | - |
| 19 | ge_proto_xxxx_PrepareAfterProcessBeforeInfershape.txt | GE | 对条件算子进行死边消除处理之后的图结构 | - |
| 20 | ge_proto_xxxx_AfterFirstInferformat.txt | GE | 经过全图inferformat处理之后的图结构 | - |
| 21 | ge_proto_xxxx_AfterInfershape.txt | GE | 经过全图infershape处理之后的图结构，会伴随常量折叠 | 白名单 |
| 22 | ge_proto_xxxx_PrepareAfterInferFormatAndShape.txt | GE | 经历完所有InferFormat与InferShape处理之后的图结构，与上图间经历了第二次全图InferFormat | - |
| 23 | ge_proto_xxxx_RunCustomPass_AfterInferShape.txt | GE | 在InferShape之后用户自定义Pass的出口图 | 白名单 |
| 24 | ge_proto_xxxx_AfterSecondInferformat.txt | GE | 第二次格式推导之后的图 | - |
| 25 | ge_proto_xxxx_PrepareAfterCtrlFlowPreProcess.txt | GE | 对条件算子做预处理之后的图结构 | - |
| 26 | ge_proto_xxxx_PrepareAfterGetDynamicOutputShape.txt | GE | 动态档位下，对图输出做处理之后的图结构 | - |
| 27 | ge_proto_xxxx_PrepareAfterProcessAippStage2.txt | GE | 在AIPP模式下，对图输入节点做相关处理之后的图结构 | - |
| 28 | ge_proto_xxxx_PrepareAfterPrepareOptimize.txt | GE | 在图准备阶段，做相关优化处理之后的图结构 | - |
| 29 | ge_proto_xxxx_PreRunAfterPrepare.txt | GE | 目前和上张图相同，经历过所有图准备处理之后的图结构 | 白名单 |
| 30 | ge_proto_xxxx_OptimizeQuantGraph_FeGraphFusionAfter.txt | FE | 图优化阶段的量化流程结束后的图结构 | - |
| 31 | ge_proto_xxxx_OptimizeOriginalGraph_FeGraphFusionAfter.txt | FE | 图融合流程结束后的图结构 | - |
| 32 | ge_proto_xxxx_OptimizeOriginalGraph_FeTopoSortingAfter.txt | FE | 图融合后进行拓扑排序，排查融合后是否成环的图结构 | - |
| 33 | ge_proto_xxxx_OptimizeOriginalGraph_DSAFeOpJudgeAfter.txt | FE | 经过动态shape分析后的格式DataType和Format选择的图 | - |
| 34 | ge_proto_xxxx_HcclBeforeOptimizeOriginalGraph.txt | HCCL | HCCL原图优化之前的图 | - |
| 35 | ge_proto_xxxx_HcclAfterOptimizeOriginalGraph.txt | HCCL | HCCL原图优化之后的图 | - |
| 36 | ge_proto_xxxx_RunCustomPassAfterBuiltinFusionPass.txt | GE | 内部Pass之后用户自定义Pass的出口图 | 白名单 |
| 37 | ge_proto_xxxx_PreRunAfterOptimizeOriginalGraph.txt | GE | 经过各算子信息库原图优化处理（OptimizeOriginalGraph接口调用）之后的图结构 | 白名单 |
| 38 | ge_proto_xxxx_PrepareAfterUpdateInputOutputByUserOptions.txt | GE | 根据用户参数，对图输入输出做相关处理之后的图结构 | - |
| 39 | ge_proto_xxxx_PrepareAfterUpdateVariableFormats.txt | GE | 对变量的Format进行相关处理之后的图结构 | - |
| 40 | ge_proto_xxxx_PreRunAfterPrepareRunningFormatRefiner.txt | GE | 与上图相同 | - |
| 41 | ge_proto_xxxx_BeforeOptimizeOriginalGraphJudgeInsert.txt | FE | op_judge流程的入口图 | - |
| 42 | ge_proto_xxxx_OptimizeOriginalGraph_FeOpDtypeJudgeAfter.txt | FE | 精度模式选择后的图 | - |
| 43 | ge_proto_xxxx_PreRunAfterRefineRunningPrecision.txt | GE | 精度选择之后的图 | - |
| 44 | ge_proto_xxxx_AfterPrecisionRefine.txt | GE | 精度选择之后经过转换算子融合生成的图 | - |
| 45 | ge_proto_xxxx_PreRunAfterAfterPrecisionRefine.txt | GE | 精度选择之后经过转换算子融合，再经过自动融合生成的图<br>如果未开启自动融合功能，该子图的内容会和<br>ge_proto_xxxx_AfterPrecisionRefine.txt子图内容一致 | - |
| 46 | ge_proto_xxxx_OptimizeOriginalGraph_FeOpFormatJudgeAfter.txt | FE | 格式选择后完整op_judge的图 | - |
| 47 | ge_proto_xxxx_OptimizeOriginalGraph_FeDistHeavyFormatAfter.txt | FE | 重型算子扩散后的图结构 | - |
| 48 | ge_proto_xxxx_OptimizeOriginalGraph_FeInsertTransNodeAfter.txt | FE | 插入转换算子后的图结构 | - |
| 49 | ge_proto_xxxx_PreRunAfterRefineRunningFormat.txt | GE | 经过各算子信息库优化处理（OptimizeOriginalGraphJudgeInsert接口调用）之后的图结构 | - |
| 50 | ge_proto_xxxx_PreRunAfterSubexpressionMigration.txt | GE | 动态分档场景下公共子表达式提取之后的图结构 | - |
| 51 | ge_proto_xxxx_before_SameTransdataBreadthFusionPass.txt | GE | SameTransdataBreadthFusionPass入口图 | - |
| 52 | ge_proto_xxxx_after_SameTransdataBreadthFusionPass.txt | GE | SameTransdataBreadthFusionPass出口图 | - |
| 53 | ge_proto_xxxx_OptimizeStage1_1.txt | GE | 图优化1_1阶段处理之后的图结构 | - |
| 54 | ge_proto_xxxx_OptimizeStage1_2.txt | GE | 图优化1_2阶段处理之后的图结构 | - |
| 55 | ge_proto_xxxx_PreRunAfterOptimize1.txt | GE | 所有图优化1阶段处理之后的图结构 | - |
| 56 | ge_proto_xxxx_PreRunAfterOptimizeAfterStage1.txt | GE | 经过各算子信息库优化处理（OptimizeAfterStage1接口调用）之后的图结构 | 白名单 |
| 57 | ge_proto_xxxx_RunCustomPassAfterOriginGraphOptimize.txt | GE | 在原图优化阶段后执行自定义pass | 白名单 |
| 58 | ge_proto_xxxx_PreRunAfterInferShape2.txt | GE | 第二次InferShape处理之后的图结构 | - |
| 59 | ge_proto_xxxx_BeforeStagePartition.txt | GE | Stage切分前的图 | - |
| 60 | ge_proto_xxxx_AfterStagePartition.txt | GE | Stage切分后的图 | - |
| 61 | ge_proto_xxxx_AfterEnginePlacer.txt | GE | 引擎选择完成后的图 | - |
| 62 | ge_proto_xxxx_Before_DSP.txt | GE | 动静模型拆分前的图 | - |
| 63 | ge_proto_xxxx_After_DSP.txt | GE | 动静模型拆分后的图 | - |
| 64 | ge_proto_xxxx_AfterDynamicShapePartition.txt | GE | 动态shape图拆分之后的图结构 | - |
| 65 | ge_proto_xxxx_MergedComputeGraphAfterCompositeEnginePartition.txt | GE | 经历对立子图拆分与子图优化处理之后的合并图结构 | - |
| 66 | ge_proto_xxxx_partition0_rank0_inputNodeGraph_AtomicEnginePartitioning.txt | GE | 原子引擎规则图拆分后，输入节点子图的图结构 | - |
| 67 | ge_proto_xxxx_partition0_rank1_new_sub_graph1_AtomicEnginePartitioning.txt | GE | 原子引擎规则图拆分后，子图1的图结构 | - |
| 68 | ge_proto_xxxx_partition0_rank2_new_sub_graph110_AtomicEnginePartitioning.txt | GE | 原子引擎规则图拆分后，子图110的图结构 | - |
| 69 | ge_proto_xxxx_OptimizeSubgraphPreProc.txt | GE | 子图优化预处理出口图 | - |
| 70 | ge_proto_xxxx_DNN_VM_RTS_OptimizeSubGraphBefore.txt | RTS | - | - |
| 71 | ge_proto_xxxx_DNN_VM_RTS_OptimizeSubGraphAfter.txt | RTS | - | - |
| 72 | ge_proto_xxxx_AIcoreEngine_OptimizeSubGraphBefore.txt | FE | AI Core子图优化入口图 | - |
| 73 | ge_proto_xxxx_OptimizeSubGraphBefore.txt | GE | 子图优化操作前的子图结构，每张子图都有一份，同名不同序号，总个数根据子图个数确定 | - |
| 74 | ge_proto_xxxx_OptimizeSubGraphAfter.txt | GE | 子图优化操作后的子图结构，每张子图都有一份，同名不同序号，总个数根据子图个数确定 | - |
| 75 | ge_proto_xxxx_partition0_rank1_new_sub_graph1_lxfusion_input.txt | AOE(lxfusion) | ATC场景和AOE baseline场景的sgat输入图 | - |
| 76 | ge_proto_xxxx_partition0_rank1_new_sub_graph1_after_rebuild.txt | AOE(lxfusion) | AOE sgat内部流程UB融合图 | - |
| 77 | ge_proto_xxxx_AIcoreEngine_OptimizeSubGraphAfter.txt | FE | AI Core子图优化出口图 | - |
| 78 | ge_proto_xxxx_OptimizeSubgraphPostProc.txt | GE | 子图优化后处理出口图 | - |
| 79 | ge_proto_xxxx_mergedComputeGraph.txt | GE | 图合并之后的图结构，与上图相同 | - |
| 80 | ge_proto_xxxx_MergedComputeGraphAfterAtomicEnginePartition.txt | GE | 经历对立原子引擎拆分与子图优化处理之后的合并图结构 | - |
| 81 | ge_proto_xxxx_PreRunAfterOptimizeSubgraph.txt | GE | 子图优化处理之后的图结构 | 白名单 |
| 82 | ge_proto_xxxx_OptimizeWholeGraphaicpu_tf_optimizer.txt | GE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 83 | ge_proto_xxxx_OptimizeWholeGraphaicpu_ascend_optimizer.txt | GE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 84 | ge_proto_xxxx_OptimizeWholeGraphdvpp_graph_optimizer.txt | GE | 整图优化DVPP优化后出口图 | - |
| 85 | ge_proto_xxxx_OptimizeWholeGraphhccl_alltoallvc_fusion_optimizer.txt | HCCL | HCCL原图优化阶段融合优化后的图 | - |
| 86 | ge_proto_xxxx_OptimizeWholeGraphAIcoreEngine.txt | GE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 87 | ge_proto_xxxx_OptimizeWholeGraphDSAEngine.txt | FE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 88 | ge_proto_xxxx_OptimizeWholeGraphhccl_graph_optimizer.txt | HCCL | HCCL原图优化阶段优化的图 | - |
| 89 | ge_proto_xxxx_OptimizeWholeGraphDNN_VM_RTS_GRAPH_OPTIMIZER_STORE.txt | GE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 90 | ge_proto_xxxx_OptimizeWholeGraphDNN_VM_RTS_FFTS_PLUS_GRAPH_OPTIMIZER_STORE.txt | RTS | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 91 | ge_proto_xxxx_OptimizeWholeGraphDNN_VM_HOST_CPU_OPTIMIZER.txt | GE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 92 | ge_proto_xxxx_OptimizeWholeGraphge_local_graph_optimizer.txt | GE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 93 | ge_proto_xxxx_OptimizeWholeGraphffts_plus.txt | FE | 调用各引擎的原图优化接口后的图信息，OptimizeWholeGraph后为引擎名称 | - |
| 94 | ge_proto_xxxx_PreRunAfterOptimizeWholeGraph.txt | GE | 经过各算子信息库优化处理（OptimizeWholeGraph接口调用）之后的图结构 | - |
| 95 | ge_proto_xxxx_PreRunAfterOptimize2.txt | GE | 所有图优化2阶段处理之后的图结构 | - |
| 96 | ge_proto_xxxx_PreRunAfterOptimizeGraphBeforeBuild.txt | GE | 模型编译入口图 | 白名单 |
| 97 | ge_proto_xxxx_PreRunAfterOptimizeTensorMove.txt | GE | 优化冗余TensorMove节点之后的图 | - |
| 98 | ge_proto_xxxx_BeforeHandleMemConflict.txt | GE | 处理内存冲突之前的图 | - |
| 99 | ge_proto_xxxx_AfterHandleMemConflict.txt | GE | 处理内存冲突之后的图 | - |
| 100 | ge_proto_xxxx_BeforeHandleMemoryLayoutConflict.txt | GE | 解决内存排布冲突入口图 | - |
| 101 | ge_proto_xxxx_PreRunAfterMemConflictProc.txt | GE | 解决内存读写冲突出口图 | - |
| 102 | ge_proto_xxxx_partition0_rank0_inputNodeGraph_SecondPartitioning.txt | GE | 二拆操作后，输入节点子图的图结构 | - |
| 103 | ge_proto_xxxx_partition0_rank1_new_sub_graph1_SecondPartitioning.txt | GE | 二拆操作后，子图1的图结构 | - |
| 104 | ge_proto_xxxx_partition0_rank2_new_sub_graph110_SecondPartitioning.txt | GE | 二拆操作后，子图110的图结构 | - |
| 105 | ge_proto_xxxx_BeforeUnfoldSubgraphs.txt | GE | 动态shape图展开之前的图 | - |
| 106 | ge_proto_xxxx_AfterUnfoldSubgraphs.txt | GE | 动态shape图展开之后的图 | - |
| 107 | ge_proto_xxxx_RunCustomPass_BeforeAssignLogicStream{pass_name}.txt | GE | 用户自定义流分配Pass处理之前的图 | 白名单 |
| 108 | ge_proto_xxxx_RunCustomPass_AfterAssignLogicStream{pass_name}.txt | GE | 用户自定义流分配Pass处理之后的图 | 白名单 |
| 109 | ge_proto_xxxx_AfterAssignResource.txt | GE | 流分配、内存分配、GenTask之后的图 | - |
| 110 | ge_proto_xxxx_Build.txt | GE | 模型编译出口图 | 白名单 |
| 111 | ge_proto_xxxx_PreRunAfterBuild.txt | GE | 与上图相同 | - |
| 112 | ge_proto_xxxx_BeforeAttrsCompress.txt | GE | 离线模型压缩前的图 | - |
| 113 | ge_proto_xxxx_AfterAttrsCompress.txt | GE | 离线模型压缩后的图 | - |
| 114 | ge_proto_xxxx_BeforeAttrsDecompress.txt | GE | 离线模型解压前的图 | - |
| 115 | ge_proto_xxxx_AfterAttrsDecompress.txt | GE | 离线模型解压后的图 | - |
| 116 | ge_proto_xxxx_ComputeGraphBeforeLowering.txt | GE | lowering前的计算图 | 白名单 |
| 117 | ge_proto_xxxx_Before_MultiStream_LoweringFirstLastEventSync.txt | GE | 多流插入Event之前的执行图 | - |
| 118 | ge_proto_xxxx_ExeGraphBeforeOptimize.txt | GE | 执行图优化前的执行图 | 白名单 |
| 119 | ge_proto_xxxx_AfterZeroCopy.txt | GE | 零拷贝优化之后的执行图 | - |
| 120 | ge_proto_xxxx_AfterCEM.txt | GE | CEM优化之后的执行图 | - |
| 121 | ge_proto_xxxx_AfterCopyFlowLaunch.txt | GE | 随路拷贝优化后的执行图 | - |
| 122 | ge_proto_xxxx_TrustOutTensorAfter.txt | GE | TrustOutTensor优化之后的执行图 | - |
| 123 | ge_proto_xxxx_AfterAicpuFuseHostInputs.txt | GE | AicpuFuseHostInputs优化之后的图 | - |
| 124 | ge_proto_xxxx_ExecuteGraphAfterSplit.txt | GE | 动态shape最终的执行图 | 白名单 |
