# GE Architecture Documentation

This documentation set introduces GE (Graph Engine) architecture design from different dimensions, **targeting developers who want to contribute code to GE**, helping quickly understand the overall project structure, core design decisions, and implementation details of each module.

## Architecture Overview

| Document | Description |
|----------|-------------|
| [GE Architecture Introduction](architecture.md) | System architecture overview, AscendIR introduction, compilation optimization, plugin extension mechanism |

## Module Architecture Documents

| Document | Description |
|----------|-------------|
| [AscendIR](modules/graph_metadef/ascend-ir.md) | Detailed design of AscendIR graph intermediate representation |
| [Compiler](modules/compiler/compiler.md) | GE Compiler compilation flow, optimization passes, engine partitioning, build stages |
| [Runtime](modules/runtime/runtime.md) | GE Executor model loading, Sink mode, Hybrid execution, v2 architecture |

## Feature Design Documents

The following documents describe cross-module feature designs:

| Document | Description |
|----------|-------------|
| [Dump Module](features/datadump.md) | Dump module overall design: architecture layering, RT1.0/RT2.0 adaptation, HCCL processing, dynamic switch |
| [External Weight](features/external_weight.md) | FileConstant feature: weight separation from OM, compile-time Const→FileConstant conversion, RT V1/V2 loading flow, memory management, global weight manager |
| [Constant Folding](features/constant_folding.md) | Constant folding optimization: compile-time constant expression evaluation, dimension calculation, empty tensor replacement, delayed effect mechanism, multi-stage compilation pipeline |
| [Fusion Pattern Pass](features/fusion_pattern_pass.md) | Fusion Pattern Pass mechanism: PatternFusionPass / DecomposePass matching, filtering, replacement, execution stages and Python/C++ integration |
| [Dynamic Gear](features/dynamic_gear.md) | Dynamic gear feature: dynamic Batch / dynamic resolution / ND arbitrary dimension modes, gear enumeration, static subgraph generation and runtime dispatch |
| [Memory Conflict Handling](features/memory_conflict.md) | Memory conflict protection system: semantic read-write conflict, memory layout conflict, subgraph address isolation, Inplace reuse conflict, multi-stream concurrency management |
| [Model Cache](features/model_cache.md) | Compilation result persistence mechanism: graph compilation cache, JIT compilation cache, operator model cache three-level system, cache hit and invalidation strategies |
| [Profiling](features/profiling.md) | Performance collection and observability: layered collection architecture (API/Host/Device), on-demand enablement, msprof unified reporting |
| [SO in OM](features/so_in_om.md) | Operator self-contained packaging: packaging dependent operator .so files into OM on demand, eliminating runtime dependency on OPP operator packages |
| [TensorMove Elimination](features/tensormove_delete.md) | TensorMove redundant node elimination optimization: identify and delete redundant memory copy nodes, O3 optimization level |
| [Variable Management](features/variable_manager.md) | Variable lifecycle management: registration, memory allocation, format conversion, logical address mapping, serialization/deserialization full flow |
| [Zero Copy](features/zero_copy.md) | Zero copy feature: input zero copy (eliminate H2D), output zero copy (eliminate D2H/D2D), compile-time planning and runtime execution |
| [Concat No Task](features/concat_no_task.md) | Concat continuous memory optimization: compile-time identification of continuous input Concat operators, mark as virtual operators to skip Task generation and memory movement |
| [GE Local Operator](features/ge_local_operator.md) | GE Local engine: dedicated engine for non-computation nodes (Data, Constant, control flow, shape transformation, etc.), zero runtime computation overhead |
| [Engine](features/engine.md) | Engine system: plugin-based engine architecture, priority-driven automatic selection, compile-time engine registration and partitioning, runtime dispatch |
| [Tiling Sink](features/tiling_sink.md) | Tiling sink feature: move Tiling computation from Host to Device AICPU execution, eliminate Host-Device synchronization overhead |
| [Graph Splitter](features/graph_splitter.md) | Graph split feature: static/dynamic Shape split, engine-level split, pipeline stage split, JIT incremental split |
| [Static Executor](features/known_shape_executor.md) | Static subgraph executor: Task Sink pre-dispatch, DavinciModel loading/execution, hybrid execution mode address refresh |
| [Dynamic Executor](features/unknown_shape_executor.md) | RT2.0 dynamic Shape executor: Lowering mechanism, ExecuteGraph, ModelV2Executor, three-subgraph lifecycle, Kernel registration system |
| [Stream Allocator](features/stream_allocator.md) | Stream allocation feature: logical stream allocation, synchronization event management, physical stream split, stream activation mechanism |
| [InferShape](features/infer_shape.md) | Shape inference: OriginShape/StorageShape dual system, compile-time InferShapePass, runtime inference node, symbolic inference |
| [Format Inference](features/infer_format.md) | Format inference: OriginFormat anchor propagation, StorageFormat automatic selection, TransData insertion optimization |

## Module Key Design Principles and Software Constraints

The following documents record key design constraints and development standards for features:

| Document | Description |
|----------|-------------|
| [Memory Module Software Constraints](constraints/memory-constraints.md) | Static/dynamic memory reuse, Allocator threading model, memory release timing, process exit cleanup |
| [RT2 Runtime Design Principles](constraints/rt2_runtime.md) | RT2 dynamic Shape module design principles: loading/execution rules, performance, compatibility, concurrency, debuggability |
| [Graph Split Module Design Principles](constraints/graph_split.md) | Graph split module design principles: responsibility boundaries, split basis, multi-threading concurrency, debugging logs, compatibility, review checklist |
| [Stream Allocator Design Principles](constraints/stream_allocator.md) | Static/dynamic Shape stream allocation design: Pass architecture, stream reuse, Event synchronization, stream activation mechanism |
| [Static Shape Runtime Design Principles](constraints/known_shape_runtime.md) | Static Shape module design principles: performance optimization, ArgsFormat, address refresh strategy, memory management |
| [Graph Foundation Structure Design Principles](constraints/graph_metadef.md) | Graph compilation common foundation structure design principles: independence, compatibility, observability, concurrency model, cross-platform consistency |
