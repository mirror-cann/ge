# Sample Usage Guide

## 1. Function Description

This sample demonstrates graph construction using operator overload, aimed at helping graph developers quickly understand operator overload definition.
Unlike `operator_overload`, this directory uses `Session.run_graph_with_stream_async` interface during execution phase.
For more information about asynchronous execution, please refer to [Asynchronous Execution](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/850/graph/graphdevg/atlasag_25_0035.html).

## 2. Directory Structure

```angular2html
python/
├── src/
|   ├── common.py                            // Common logic: constants, graph construction, input tensors, GE lifecycle
|   ├── make_add_graph.py                    // Asynchronous execution sample
|   └── make_add_graph_custom_allocator.py   // Register custom SamplePoolAllocator asynchronous execution sample
├── README.md               // README file
├── run_sample.sh           // Execution script
```

## 3. Usage Instructions

### 3.1. Prepare CANN Package

- This sample requires installing two sets of CANN simultaneously: latest development package for graph compilation, officially released package from website provides [`pyACL`](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/appdevg/acldevg/aclpythondevg_0096.html) module for graph execution. "Compilation" and "execution" in this text specifically refer to graph compilation and graph execution, not GE engineering source code compilation.
- Installation please refer to:
  - Latest development package, for graph compilation, provides latest GE/Python capabilities required by this sample. Installation please refer to [Environment Preparation](../../../../docs/zh/quick_install.md) "Method 3: Manual installation of software packages > Scenario 1: Experience master version capabilities or develop based on master version", install latest version of `toolkit` and `ops` packages
  - Officially released CANN `toolkit` and `ops` packages, for graph execution, provide `pyACL`. Installation please refer to [Environment Preparation](../../../../docs/zh/quick_install.md) "Method 3: Manual installation of software packages > Scenario 2: Experience released version capabilities or develop based on released version", install officially released software packages
- Set environment variables (assuming latest development package is installed at /usr/local/Ascend/, officially released package is installed at /usr/local/Ascend-release/)

```bash
source /usr/local/Ascend/cann/set_env.sh
export PYTHONPATH="$PYTHONPATH:/usr/local/Ascend-release/cann/python/site-packages"
```

### 3.2. Execute

This directory provides two optional targets:

#### 3.2.1. Default allocator sample

```bash
bash run_sample.sh -t sample_and_run_python
```

This command will:

1. Generate dump graph and use GE built-in allocator to asynchronously execute the graph

After successful execution, you will see:

```text
[Success] sample execution successful, pbtxt dump generated in current directory. The file starts with ge_onnx_ and can be opened in netron for display
```

#### 3.2.2. Custom allocator sample

```bash
bash run_sample.sh -t sample_and_run_python_custom_allocator
```

This command on basis of default sample, through `Session.register_external_allocator` registers custom `SamplePoolAllocator` to GE. This class inherits `ge.allocator.Allocator` base class: maintains free block list by **exact byte count**, hits then reuse, otherwise `acl.rt.malloc` allocate; `free` time puts block back to pool (not immediately `acl.rt.free`). When object is recycled, in `__del__` uniformly `acl.rt.free` cached blocks.

After successful execution, terminal will show output in following format (specific line count depends on GE allocation/release for output Tensor; address and `size` based on actual execution):

```text
[Info] SamplePoolAllocator registered to stream
[SamplePool] new   : addr=0x<device memory address>  size=<byte count> B
[Info] Asynchronous Graph execution successful!
[SamplePool] cache : addr=0x<device memory address>  size=<byte count> B
[Info] SamplePoolAllocator unregistered
[SamplePool] destroy: freed <N> cached blocks
[Info] Running environment cleaned
[Success] sample execution successful, pbtxt dump generated in current directory. The file starts with ge_onnx_ and can be opened in netron for display
```

#### Output File Description

After successful execution, the following files will be generated in current directory:

- `ge_onnx_*.pbtxt` - protobuf text format of graph structure, can be viewed with netron

### 3.3. Log Printing

If you need log printing to assist debugging during executable program execution, you can set the following environment variables before `bash run_sample.sh -t sample_and_run_python` to print logs to screen:

```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1 # Print logs to screen
export ASCEND_GLOBAL_LOG_LEVEL=0     # Log level set to debug level
```

### 3.4. DUMP Graph During Graph Compilation Process

If you need to DUMP graph to assist debugging graph compilation process during executable program execution, you can set the following environment variables before `bash run_sample.sh -t sample_and_run_python` to DUMP graph to execution path:

```bash
export DUMP_GE_GRAPH=2
```

## 4. Core Concepts Introduction

### 4.1. Graph Construction Steps

- Create graph builder (provides context, workspace and construction-related methods needed for graph construction)
- Add starting nodes (starting nodes refer to nodes without input dependencies, usually including graph inputs (like Data nodes) and weight constants (like Const nodes))
- Add intermediate nodes (intermediate nodes are computation nodes with input dependencies, usually generated by user graph construction logic, and connected using existing nodes as inputs)
- Set graph output (explicitly specify graph output nodes as computation result endpoints)

### 4.2. Operator Overload

**Concept Explanation:**
Operator overload is a syntax sugar provided by ES API, with syntax encapsulation for AI operators, making graph construction code more concise and intuitive.

**Graph Construction API Features:**

- Operator overload maintains the same type checking and constraints as function calls
