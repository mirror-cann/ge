# Sample Usage Guide

## 1. Function Description

This sample demonstrates the offline graph compilation and execution workflow. For more information about compiling graphs into offline models, refer to [Generating Offline Models](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/graph/graphdevg/atlasag_25_0030.html).

## 2. Directory Structure

```text
cpp/
├── CMakeLists.txt           // CMake build file
├── main.cpp                 // Program main entry
├── run_sample.sh            // Execution script
├── README.md                // README file
└── src/
    ├── CMakeLists.txt            // CMake build file
    ├── common.h / common.cpp     // Common logic files
    ├── single_model/             // Single model compilation and inference
    └── bundle_model/             // Bundle compilation and inference
```

## 3. Usage

### 3.1 Prepare CANN Package

- Correctly install `toolkit` and `ops` packages through [Environment Preparation](../../../docs/zh/quick_install.md) section "Method 3: Manual Package Installation > Scenario 1: Experience master version capabilities or develop based on master version"
- Set environment variables (assuming package is installed in /usr/local/Ascend/)

```bash
source /usr/local/Ascend/cann/set_env.sh 
```

### 3.2 Graph Compilation and Execution

Execute single model sample:

```bash
bash run_sample.sh -t sample_and_run
```

This command will:

1. Compile C++ executable program
2. Build `Add` graph, compile offline and generate `add_sample.om`
3. Load and execute the offline model

Execute bundle sample:

```bash
bash run_sample.sh -t sample_and_run_bundle
```

This command will:

1. Compile C++ executable program
2. Bundle `Add` graph and `Mul` graph, compile offline and generate `bundle_sample.om`
3. Load Bundle and execute two sub-models separately

For offline compilation in cardless scenarios where you need to specify target chip version, add `--soc-version`:

```bash
bash run_sample.sh -s Ascend910B1 -t sample_and_run
bash run_sample.sh -s Ascend910B1 -t sample_and_run_bundle
```

You can also split into "graph compilation only" and "graph execution only" phases:

```bash
bash run_sample.sh -t build_model
bash run_sample.sh -t run_infer

bash run_sample.sh -t build_bundle_model
bash run_sample.sh -t run_bundle_infer
```

> `run_infer` / `run_bundle_infer` requires om model to already exist

After successful execution you will see:

```text
[Success] sample execution successful
```

#### Output Files Description

After successful execution, the following files are generated in the current directory:

- `add_sample.om` - Single model offline file
- `bundle_sample.om` - Bundle offline model file

### 3.3 Log Printing

If you need log printing to help troubleshoot during executable program execution, you can set the following environment variables before `bash run_sample.sh` to print logs to screen

```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1 # Print logs to screen
export ASCEND_GLOBAL_LOG_LEVEL=0 # Log level is debug
```

## 4. Core Workflow Introduction

### 4.1 Single Model Offline Compilation and Execution

- Use `aclgrphBuildInitialize` to initialize compilation environment
- Build `Graph` and generate offline model via `aclgrphBuildModel`
- Save `om` file using `aclgrphSaveModel`
- Execute offline model via `aclmdlLoadFromFile`, `aclmdlExecute`

### 4.2 Bundle Offline Compilation and Execution

- Organize Bundle using multiple `Graph`
- Build Bundle model at once via `aclgrphBundleBuildModel`
- Save `bundle_sample.om` using `aclgrphBundleSaveModel`
- Load Bundle via `aclmdlBundleLoadFromFile` and execute sub-models sequentially
