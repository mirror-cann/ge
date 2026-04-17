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
