# GE 架构文档

本文档集从不同维度介绍 GE (Graph Engine) 的架构设计，**面向希望为 GE 贡献代码的开发者**，帮助快速理解项目整体结构、核心设计决策和各模块的实现细节。

排除测试后约 **79 万行有效代码**，8千余个源文件：

| 模块 | 占比 | 职责 |
|------|------|------|
| compiler | 34% | 图编译（优化、分区、构建） |
| runtime | 20% | 图执行（加载、执行、Hybrid） |
| graph_metadef | 12% | IR 定义、算子注册 |
| dflow | 11% | 分布式流框架 |
| base | 7% | 基础设施 |
| inc | 6% | 公共头文件 |
| api | 4% | API 接口 |
| parser | 3% | 模型解析 |

## 架构总览

| 文档 | 说明 |
|------|------|
| [GE 架构介绍](architecture.md) | 系统架构总览、AscendIR 介绍、编译优化、插件扩展机制 |

## 模块架构文档

| 文档 | 说明 |
|------|------|
| [AscendIR](modules/graph_metadef/ascend-ir.md) | AscendIR 图中间表示的详细设计 |
| [编译器](modules/compiler/compiler.md) | GE Compiler 的编译流程、优化 Pass、引擎分区、构建阶段 |
| [运行时](modules/runtime/runtime.md) | GE Executor 的模型加载、Sink 模式、Hybrid 执行、v2 架构 |

## 特性设计文档

以下文档描述跨模块的特性设计：

| 文档 | 说明 |
|------|------|
| [Dump 模块](features/datadump.md) | Dump 模块整体设计：架构分层、RT1.0/RT2.0 适配、HCCL 处理、动态开关 |
| [外置权重](features/external_weight.md) | FileConstant 特性：权重从 OM 分离存储、编译期 Const→FileConstant 转换、RT V1/V2 加载流程、内存管理、全局权重管理器 |
| [常量折叠](features/constant_folding.md) | 常量折叠优化：编译期常量表达式求值、维度计算、空张量替换、延迟生效机制、多编译阶段流水线 |
| [动态分档](features/dynamic_gear.md) | 动态分档特性：动态 Batch / 动态分辨率 / ND 任意维度三种模式、档位枚举、静态子图生成与运行时分发 |
| [内存冲突处理](features/memory_conflict.md) | 内存冲突防护体系：语义读写冲突、内存布局冲突、子图地址隔离、Inplace 复用冲突、多流并发管理 |
| [模型缓存](features/model_cache.md) | 编译结果持久化机制：图编译缓存、JIT 编译缓存、算子模型缓存三级体系、缓存命中与失效策略 |
| [Profiling](features/profiling.md) | 性能采集与可观测性：分层采集架构（API/Host/Device）、按需使能、msprof 统一上报 |
| [SO in OM](features/so_in_om.md) | 算子自包含打包：将依赖的算子 .so 按需打包进 OM 文件、消除运行时对 OPP 算子包的依赖 |
| [TensorMove 消除](features/tensormove_delete.md) | TensorMove 冗余节点消除优化：识别并删除冗余内存拷贝节点、O3 优化级别 |
| [变量管理](features/variable_manager.md) | 变量生命周期管理：注册、内存分配、格式转换、逻辑地址映射、序列化/反序列化全流程 |
| [零拷贝](features/zero_copy.md) | 零拷贝特性：输入零拷贝（消除 H2D）、输出零拷贝（消除 D2H/D2D）、编译期规划与运行时执行 |
| [Concat No Task](features/concat_no_task.md) | Concat 连续内存优化：编译期识别输入连续的 Concat 算子，标记为虚拟算子跳过 Task 生成和内存搬运 |

## 模块关键设计原则与软件约束

以下文档记录特性的关键设计约束和开发规范：

| 文档 | 说明 |
|------|------|
| [内存模块软件约束](constraints/memory-constraints.md) | 静态/动态内存复用、Allocator 线程模型、内存释放时序、进程退出清理 |
| [RT2 运行时设计原则](constraints/rt2_runtime.md) | RT2 动态 Shape 模块设计原则：加载/执行规则、性能、兼容性、并发、可调试性 |
| [图拆分模块设计原则](constraints/graph_split.md) | 图拆分模块设计原则：职责边界、拆分依据、多线程并发、维测日志、兼容性、评审检查清单 |
| [流分配模块设计原则](constraints/stream_allocator.md) | 静态/动态 Shape 流分配设计：Pass 架构、流复用、Event 同步、流激活机制 |
| [静态 Shape 运行时设计原则](constraints/known_shape_runtime.md) | 静态 Shape 模块设计原则：性能优化、ArgsFormat、地址刷新策略、内存管理 |
| [图基础结构设计原则](constraints/graph_metadef.md) | 图编译公共基础结构设计原则：独立性、兼容性、可观测性、并发模型、跨平台一致性 |
