# AGENTS.md

This file provides guidance for agents working in this code repository.

## Project Overview

GE (Graph Engine) is Huawei CANN's (Compute Architecture for Neural Networks) graph engine - a high-performance graph compiler and executor for Ascend AI processors. GE provides graph optimization, multi-stream parallel execution, memory reuse optimization, and model sinking capabilities.

### Key Directories

| Directory | Purpose |
|------|------|
| `api/` | Public API interfaces (ACL, ATC, session, Python bindings) |
| `base/` | Base components (graph structure, utilities, host CPU engine) |
| `compiler/` | Graph compilation (analyzers, engines, graph compiler, operator compiler) |
| `runtime/` | Runtime execution (V1 static executor, V2 dynamic executor RT2.0) |
| `dflow/` | Distributed flow framework (LLM data distribution, UDF) |
| `parser/` | Model format parsers (ONNX, PB, Caffe, MindSpore) |
| `graph_metadef/` | Graph metadata definitions and operator registration |
| `tests/` | Comprehensive test suite (UT, ST, benchmarks) |
| `examples/` | Usage examples and samples |

## Build Commands

### Basic Build

```bash
# Build all components (ge_compiler, ge_executor, dflow)
bash build.sh

# Build specific components
bash build.sh --ge_compiler    # Build compiler package
bash build.sh --ge_executor    # Build executor package
bash build.sh --dflow          # Build dflow package
```

### Build Options

```bash
bash build.sh -j16                  # 16 parallel threads (default: 8)
bash build.sh --build-type=Debug    # Debug build (default: Release)
bash build.sh --verbose             # Verbose output
bash build.sh --asan                # Enable AddressSanitizer
bash build.sh --cov                 # Enable code coverage
bash build.sh --output_path=<PATH>  # Set custom output path
```

## Testing

### DT Test Development

**Use Skill**: `ge-dt-developer`
**Applicable Scenarios**: Writing and adding UT/ST test cases.

### GE Unit Tests / System Tests

**Use Skill**: `ge-dt-runner`
**Applicable Scenarios**: Compiling and running GE project unit tests (UT) and system tests (ST).

### Clean Build Artifacts

**Not necessary, avoid cleaning. Use incremental compilation whenever possible to save 90% compilation time**

```bash
rm -rf build_ut/ build_st/ output/ build/ build_out/ cov/ build_cmake_gcov/
```

## Feature Development and New Features
>
> **Trigger Words**: Add new feature/requirement/capability, develop new feature/requirement/capability, implement feature/requirement/capability

**Use Skill**: `superpower brainstorming skill`

## Architecture Document Loading

**Important**: When exploring the repository/project, answering questions, modifying code, outputting design documents, requirement specs, or conducting code reviews, load corresponding documents according to the table below. Each document only needs to be loaded once, triggered by any matching trigger word, involved directory, or scenario. Also, after modifying code, update corresponding documents under `docs/en/design/features/` and `docs/en/design/modules`.

| Document | Trigger Words | Involved Directories |
|------|----------------|----------|
| [`architecture.md`](docs/en/design/architecture.md) | Architecture overview, overall design, GE introduction, system architecture, compilation optimization, plugin extension | First time understanding the project |
| [`ascend-ir.md`](docs/en/design/modules/graph_metadef/ascend-ir.md) | AscendIR, graph structure, operator registration, Anchor, DAG | `base/`, `inc/` (graph structure related) |
| [`compiler.md`](docs/en/design/modules/compiler/compiler.md) | Compiler, optimization pass, fusion, engine partition, operator compilation | `compiler/` (non memory/split/stream subdirectories) |
| [`runtime.md`](docs/en/design/modules/runtime/runtime.md) | Dynamic runtime executor, model loading, model execution, Hybrid, v2 architecture | `runtime/` (overall architecture level) |
| [`fusion_pattern_pass.md`](docs/en/design/features/fusion_pattern_pass.md) | Fusion Pattern Pass, PatternFusionPass, DecomposePass, custom fusion Pass, MeetRequirements, CaptureTensor, Replacement, PatternMatcherConfig | `compiler/graph/fusion/`, `compiler/graph/passes/feature/`, `examples/fusion_pass/` |
| [`datadump.md`](docs/en/design/features/datadump.md) | dump, overflow, disk write, datadump, exception dump | `common/dump/`, `runtime/*/dump/` |
| [`external_weight.md`](docs/en/design/features/external_weight.md) | External weight, external weight, FileConstant, weight separation, weight disk write | Use trigger words only |
| [`constant_folding.md`](docs/en/design/features/constant_folding.md) | Constant folding, constant folding, constant folding optimization, constant expression evaluation | `*constant_folding*` |
| [`dynamic_gear.md`](docs/en/design/features/dynamic_gear.md) | Dynamic gear, dynamic gear, gear, dynamic batch, dynamic resolution, dynamic_dims | `compiler/graph/preprocess/`, `compiler/graph/passes/multi_batch/`, `runtime/v1/executor/` |
| [`memory_conflict.md`](docs/en/design/features/memory_conflict.md) | Memory conflict, memory layout conflict, read-write conflict, Inplace conflict, subgraph address isolation | `compiler/graph/passes/memory_conflict/`, `mem_rw_conflict_optimize.cc`, `compiler/graph/optimize/mem_layout_conflict_optimize/`, `mem_inplace.cc` |
| [`model_cache.md`](docs/en/design/features/model_cache.md) | Model cache, model cache, compilation cache, OM cache, JIT cache | `*model_cache*` |
| [`profiling.md`](docs/en/design/features/profiling.md) | profiling, performance analysis, performance collection, msprof, performance tuning | `*profiling*` |
| [`so_in_om.md`](docs/en/design/features/so_in_om.md) | SO in OM, operator packaging, so packaging, self-contained model, operator dependency | `*op_so_store*` |
| [`tensormove_delete.md`](docs/en/design/features/tensormove_delete.md) | TensorMove elimination, TensorMove deletion, redundant copy elimination | `*tensor_move_delete*` |
| [`variable_manager.md`](docs/en/design/features/variable_manager.md) | Variable management, Variable, variable memory, VarRef, variable lifecycle, constant, FileConstant, external weight | `*var_manager*`, `*variable_optimize*` |
| [`zero_copy.md`](docs/en/design/features/zero_copy.md) | Zero copy, zero copy, model input/output, user input/output, user memory/address | `*zero_copy*` |
| [`concat_no_task.md`](docs/en/design/features/concat_no_task.md) | Concat No Task, concat optimization, continuous memory concatenation, virtual operator, no Task generation | `*concat_notask*` |
| [`ge_local_operator.md`](docs/en/design/features/ge_local_operator.md) | GE Local operator, local operator, GeLocal engine, NoOp, GeDeletedOp, PhonyConcat, PhonySplit | `*local_engine*`, `*ge_local*` |
| [`engine.md`](docs/en/design/features/engine.md) | Engine, Engine, engine selection, engine registration, engine partition, EnginePlacer, EnginePartitioner, DNNEngine, OpsKernelInfoStore | `compiler/engines/`, `*engine_place*`, `*dnnengine*` |
| [`tiling_sink.md`](docs/en/design/features/tiling_sink.md) | Tiling sink, tiling sink, AICPU Tiling, tiling_schedule_optimize | `*tiling_sink*`, `*fe_gentask_utils*` |
| [`graph_splitter.md`](docs/en/design/features/graph_splitter.md) | Graph split, Graph Split, dynamic-static split, DynamicShapePartitioner, EnginePartitioner, cluster, PartitionedCall | `compiler/graph/partition/`, `*dynamic_shape_partition*` |
| [`known_shape_executor.md`](docs/en/design/features/known_shape_executor.md) | Static executor, Known Shape Executor, Task Sink, DavinciModel, address refresh, model sink | `runtime/v1/graph/load/model_manager/` |
| [`unknown_shape_executor.md`](docs/en/design/features/unknown_shape_executor.md) | Dynamic executor, Unknown Shape Executor, RT2.0, Lowering, ExecuteGraph, ModelV2Executor, dynamic shape execution | `runtime/v2/`, `runtime/v1/hybrid/executor/` |
| [`stream_allocator.md`](docs/en/design/features/stream_allocator.md) | Stream allocation, stream, multi-stream, stream reuse, event synchronization, stream activation | `compiler/graph/build/stream/` |
| [`infer_shape.md`](docs/en/design/features/infer_shape.md) | InferShape, Shape inference, OriginShape, StorageShape, dynamic Shape, symbolic inference | `*infer_shape*`, `*symbolic_shape*` |
| [`infer_format.md`](docs/en/design/features/infer_format.md) | Format inference, Format inference, InferFormat, OriginFormat, StorageFormat, TransData, format propagation | `*format_refiner*`, `*format_optimize*` |

| Key Feature Design Principles and Software Constraints | Trigger Words | Involved Directories |
|------|----------------|----------|
| [`memory-constraints.md`](docs/en/design/constraints/memory-constraints.md) | Memory, memory reuse, block_mem, allocator, zero copy, continuous memory, memory layout conflict, memory release | `compiler/graph/build/memory/`, `compiler/graph/optimize/mem_layout_conflict_optimize/` |
| [`rt2_runtime.md`](docs/en/design/constraints/rt2_runtime.md) | RT2, dynamic shape, rt2 executor, hybrid execution | `runtime/v2/` |
| [`known_shape_runtime.md`](docs/en/design/constraints/known_shape_runtime.md) | Static shape, known shape, davinci model, sink mode, address refresh | `runtime/v1/` |
| [`graph_split.md`](docs/en/design/constraints/graph_split.md) | Graph split, graph cutting, cluster, dynamic graph split, executor selection | `compiler/graph/split/` |
| [`stream_allocator.md`](docs/en/design/constraints/stream_allocator.md) | Stream allocation, stream, multi-stream, stream reuse, event synchronization, stream activation | `compiler/graph/build/stream/` |
| [`graph_metadef.md`](docs/en/design/constraints/graph_metadef.md) | Graph basic structure | `graph_metadef/` |

## Development Standards

**gitcode pr/issue/ci operations**
@.claude/skills/default-skills/SKILL.md

### Code Review Checklist
>
> **Trigger Words**: Review code, review pr

**Use Skill**: `ge-code-reviewer`

### Design Document Checklist

> **Trigger Words**: Design document, design spec, spec output, design document, design solution output, brainstorming output document, write design document, write spec, write design, save spec, save spec, save design, write to docs/superpowers/specs, design solution, architecture design, technical solution

Any scenario that outputs design documents/specs (including but not limited to superpowers brainstorming skill, user directly requesting design document writing, design solution output), **must** first read the template file [docs/en/design/design_document_template.md], then output according to the template format. Each section of the template must be covered. Even if superpowers skill has its own format requirements, this template must be followed.

Also, **must** check the following items one by one:

- [ ] **Cross-feature impact (cross-feature-check)**: For all modules/directories involved in the design solution, **must** first read [cross_feature_check.md](docs/en/design/cross_feature_check.md), and analyze each scenario according to its guidance. Evaluate item by item according to the scenario table in cross_feature_check.md whether there are missing features/scenarios, and explicitly state in the design document
- [ ] **Key feature design principles and software constraints**: Based on the directories involved in the design solution, load corresponding architecture documents from the table above "Key Feature Design Principles and Software Constraints", ensure the design solution is consistent with existing constraints. If existing constraints need to be broken, must explicitly state the reason and impact scope

**Example output format**:

```text
### Design Document Review Results
- [x] Cross-feature impact: Analyzed each scenario according to cross_feature_check.md,
      solution involves runtime/v2/ and compiler/graph/build/memory/,
      checked rt2_runtime.md and memory-constraints.md, solution is compatible with existing memory model,
      does not affect RT2 dynamic shape execution flow
- [x] Key feature design principles: Loaded rt2_runtime.md, solution follows hybrid execution constraints,
      no breaking of existing design principles
```

### Code Style

- Follow Google open source code standards
- if/for/while/do-while statements should use braces
- Use `GetPeerInDataAnchorsPtr` instead of `GetPeerInDataAnchors`, the former does not need to construct smart pointers and has better performance. Similarly for `GetNamePtr` and `GetName`, prefer interfaces that do not return smart pointers.

## Language

Answer questions in Chinese

## Think Before Coding

**Do not assume. Do not hide confusion. Put trade-offs on the table.**

Before implementation:

- Clearly state your assumptions. If uncertain, ask.
- If multiple interpretations exist, list all of them - do not silently choose one.
- If a simpler solution exists, say it. Raise objections when necessary.
- If unclear areas exist, stop. Point out the confusion. Ask questions.

## Simplicity First

**Solve problems with minimal code. Do not make speculative designs.**

- Do not implement features beyond requirements.
- Do not make abstractions for one-time code.
- Do not add unrequested "flexibility" or "configurability".
- Do not do error handling for impossible scenarios.
- If you wrote 200 lines when 50 lines are enough, rewrite.

Ask yourself: "Would a senior engineer think this is too complex?" If yes, simplify it.

## Precise Modifications

**Only change what must be changed. Only clean up what you messed up.**

When editing existing code:

- Do not "improve" adjacent code, comments, or formatting.
- Do not refactor things that are not broken.
- Match existing style, even if you would write it differently.
- If you notice unrelated dead code, point it out - do not delete it.

When your modification creates orphaned code:

- Delete imports/variables/functions that became unused due to your modification.
- Do not delete dead code that existed before, unless requested.

Test standard: Every modification should be traceable to the user request.

## Goal-Driven Execution

**Define success criteria. Iterate until goals are met.**

Transform tasks into verifiable goals:

- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix bug" → "Write a test that reproduces the bug, then make it pass"
- "Refactor X" → "Ensure tests pass before and after refactoring"

For multi-step tasks, briefly state the plan:

```text
1. [Step] → Verify: [Check item]
2. [Step] → Verify: [Check item]
3. [Step] → Verify: [Check item]
```

Clear success criteria enable independent iteration. Vague criteria ("make it work") require continuous communication.
