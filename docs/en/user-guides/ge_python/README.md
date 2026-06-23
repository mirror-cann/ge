# GE-PY Python Module Documentation

## Overview

GE-PY is the Python interface module for GraphEngine, providing Pythonic graph-related interfaces. It offers convenient capabilities for graph construction, manipulation, compilation, and execution. The module's public header files are located in the `api/python/ge/ge/` directory.

GE-PY module contains the following core components:

- **graph module** - Graph basic operation module, providing core classes such as Graph, Node, Tensor, TensorDesc
- **passes module** - Custom Fusion Pass development module, providing Python-level graph fusion Pass development capabilities
- **ge_global module** - GE global initialization and destruction interfaces
- **error module** - Error types thrown when GE Python API fails, carrying GE internal error information and interface context
- **offline_compile module** - Offline graph compilation interfaces
- **session module** - Graph compilation execution interfaces
- **allocator module** - Memory allocator abstraction for registering external allocators in asynchronous execution scenarios
- **utils module** - GE common utility interfaces, providing Shape derivation, node AICore support validation, and other capabilities
- **es module** - Eager-Style graph construction interfaces, providing functional-style graph construction methods
- **pyatc module** - `atc` command-line equivalent entry, facilitating specifying Python interpreter within ATC process

## Documentation Navigation

### Design Documents

- **[GE-PY Module Class Relationship Document](../../design/modules/ge_python/ge_python.md)** - Detailed explanations of Graph, Node, Tensor, TensorDesc, Session and other basic modules
  - offline_compile module: Offline graph compilation interfaces
  - Graph class: Main interface for graph operations
  - Node class: Graph node operation interface
  - Tensor class: Tensor data class
  - Shape/TensorDesc class: Tensor shape and metadata description
  - GeApi class: GE initialization and destruction
  - GeError class: GE Python API error types
  - Session class: Graph compilation execution interface
  - Allocator class: External memory allocator interface for asynchronous execution scenarios
  - GeUtils class: Shape derivation and node AICore support validation utility interface

- **[ES-PY Module Document](../es_graph/api/es_python.md)** - Detailed explanation of Eager-Style graph construction module
  - GraphBuilder class: Eager-Style graph builder
  - TensorHolder class: Tensor holder

- **[Python Pass Design Document](../../design/modules/ge_python/ge_python_pass_design.md)** - Detailed design explanation of Python custom Fusion Pass development capabilities
  - FusionBasePass class: Base class for basic fusion Pass
  - PatternFusionPass class: Base class for pattern matching fusion Pass
  - DecomposePass class: Base class for operator decomposition Pass
  - Pass registration and discovery mechanism
  - Bridge architecture, native helper, multi-version artifact loading and runtime fallback codegen design

- **[pyatc CLI Design Document](../../design/modules/ge_python/pyatc_cli_design.md)** - Detailed design explanation of pyatc command line

### API Reference

- **API Reference Documents** - Interface reference documents for each module
  - [Graph](api/Graph.md), [Node](api/Node.md), [Tensor](api/Tensor.md), [TensorDesc](api/TensorDesc.md),
    [Shape](api/Shape.md), [DataType](api/DataType.md)
  - [Session](api/Session.md), [Allocator](api/Allocator.md), [GeApi](api/GeApi.md), [Error](api/Error.md)
  - [GraphBuilder](api/GraphBuilder.md), [TensorHolder](api/TensorHolder.md)
  - [OfflineCompile](api/OfflineCompile.md), [GeUtils](api/GeUtils.md)
  - [Passes](api/Passes.md), [pyatc](api/pyatc.md)

### Environment Variables

- **[ASCEND_GE_PY_PASS_PATH](env/ASCEND_GE_PY_PASS_PATH.md)** - Python Pass plugin path discovery environment variable

## Module Relationships

- **graph module** - Provides basic graph operation capabilities, serving as the foundation for other modules
- **passes module** - Provides custom Fusion Pass development capabilities, registers Passes through decorators, automatically discovered and executed by GE during compilation phase
- **es module** - Provides functional graph construction methods, ultimately builds Graph objects from the graph module
- **allocator module** - Provides external memory allocation capabilities by stream dimension for session asynchronous execution
- **utils module** - Provides common utility capabilities for graph module objects, used for Shape derivation and node support validation on replacement graphs in Python pass scenarios
- **session module** - Compiles and executes graphs built using graph module, loads and executes Passes registered in passes module during compilation
- **ge_global module** - Provides global initialization and resource management
- **error module** - Provides unified GE Python API error types, failure exceptions include GE internal error information and interface context
- **offline_compile module** - Provides offline model construction and export capabilities
- **pyatc module** - Provides command-line entry equivalent to `atc`, facilitating specifying Python interpreter within ATC process

## Usage Examples

### Basic Graph Operation Example

Refer to the [using es python api graph construction sample](../../../../examples/es/transformer/python/README.md) execution method, especially important to note:
[Need to install package first and set corresponding environment variables](../../../../examples/es/transformer/python/README.md#31准备cann包)

### Offline Graph Compilation Execution Example

Refer to the [using offline_compile python api offline graph compilation execution sample](../../../../examples/offline_compile_run/python/README.md) execution method, especially important to note:
[Need to install package first and set corresponding environment variables](../../../../examples/offline_compile_run/python/README.md#31准备cann包)

### More Examples

For more Python use cases, refer to subdirectories under [examples/es](../../../../examples/es):

## Development Roadmap

We first introduced the `ge-python` module in 2025, aiming to provide Python language graph construction, graph compilation, and graph execution capabilities.
In 2026 Q1, our main work focused on completing es api integration, allowing users to use Python es api graph construction capabilities after installing ops package:

---

### Core Architecture

- [x] [***December 2025***] `ge-python` module has completed design and implementation, providing basic capabilities for graph construction using es api, graph compilation, and graph execution.

### Basic API Integration

- [x] [***December 2025***] Basic interfaces have completed design and implementation.
- [x] [***February 2026***] es Python operator api support, see [es api integration roadmap](../es_graph/README.md#api-integration).
- [x] [***April 2026***] Graph asynchronous execution Python interface provided
- [x] [***April 2026***] Offline graph compilation execution Python interface provided
- [x] [***April 2026***] pyatc interface provided

### Custom Pass

- [x] [***April 2026***] Development mode main chain completed `FusionBasePass` `PatternFusionPass` `DecomposePass`
- [x] [***May 2026***] Pre-built version, multi-version native artifact completed
- [x] [***June 2026***] fallback codegen capability completed

### Sample and Related Documentation

- [x] [***December 2025***] Corresponding samples provided, covering common usage scenarios.
- [x] [***December 2025***] Detailed documentation provided, i.e., this directory.

### Backward Compatibility

- [x] [***December 2025***] Python api backward compatibility design and implementation completed.

### Others
- [] [***Future Phase***] Custom operator graph insertion Python support