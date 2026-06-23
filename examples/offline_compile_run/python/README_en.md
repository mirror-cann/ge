# Sample Usage Guide

## 1. Function Description

This sample demonstrates the offline graph compilation and execution workflow. For more information about compiling graphs into offline models, refer to [Generating Offline Models](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/graph/graphdevg/atlasag_25_0030.html).

## 2. Directory Structure

```angular2html
python/
├── src/
|   ├── single_model/               // Single model sample
|   |   ├── build_add_model.py      // Offline compile Add graph, generate add_sample.om
|   |   └── run_add_model.py        // Load add_sample.om and run inference
|   ├── bundle_model/               // Bundle model sample
|   |   ├── build_bundle_model.py   // Bundle Add/Mul multiple graphs, generate bundle_sample.om
|   |   └── run_bundle_model.py     // Load bundle_sample.om, execute sub-models sequentially
|   └── common.py                   // Common logic
├── README.md                       // README file
├── run_sample.sh                   // Execution script
```

## 3. Usage

### 3.1 Prepare CANN Package

- This sample requires installing two sets of CANN: the latest development package for graph compilation, and the official release package from the website providing [`pyACL`](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/appdevg/acldevg/aclpythondevg_0096.html) module for graph execution. "Compilation" and "execution" in this document specifically refer to graph compilation and graph execution, not GE source code compilation.
- Installation instructions:
  - Latest development package, for graph compilation, providing the latest GE/Python capabilities this sample depends on. Refer to [Environment Preparation](../../../docs/zh/quick_install.md) section "Method 3: Manual Package Installation > Scenario 1: Experience master version capabilities or develop based on master version", install the latest `toolkit` and `ops` packages
  - Official CANN `toolkit` and `ops` packages released on the website, for graph execution, providing `pyACL`. Refer to [Environment Preparation](../../../docs/zh/quick_install.md) section "Method 3: Manual Package Installation > Scenario 2: Experience released version capabilities or develop based on released version", install official release version software packages
- Set environment variables (assuming latest development package installed in /usr/local/Ascend/, official release package installed in /usr/local/Ascend-release/)

```bash
source /usr/local/Ascend/cann/set_env.sh
export PYTHONPATH="$PYTHONPATH:/usr/local/Ascend-release/cann/python/site-packages"
```

### 3.2 Graph Compilation and Execution

Execute single model sample:

```bash
bash run_sample.sh -t sample_and_run_python
```

This command will:

1. Build `Add` graph, compile offline and generate `add_sample.om`
2. Load and execute the offline model via pyACL

Execute bundle sample:

```bash
bash run_sample.sh -t sample_and_run_bundle_python
```

This command will:

1. Bundle `Add` graph and `Mul` graph, compile offline and generate `bundle_sample.om`
2. Load Bundle via pyACL and execute two sub-models separately

For offline compilation in cardless scenarios where you need to specify target chip version, add `--soc-version`:

```bash
bash run_sample.sh --soc-version Ascend910B1 -t sample_and_run_python
bash run_sample.sh --soc-version Ascend910B1 -t sample_and_run_bundle_python
```

You can also split into "graph compilation only" and "graph execution only" phases:

```bash
bash run_sample.sh -t build_model
bash run_sample.sh -t run_infer

bash run_sample.sh -t build_bundle_model
bash run_sample.sh -t run_bundle_infer
```

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

- Use `build_initialize` to initialize compilation environment
- Build `Graph` and generate offline model via `build_model`
- Save `om` file using `save_model`
- Execute offline model via `acl.mdl.load_from_file`, `acl.mdl.execute`

### 4.2 Bundle Offline Compilation and Execution

- Organize multiple `Graph` using `GraphWithOptions`
- Build Bundle model at once via `bundle_build_model`
- Save `bundle_sample.om` using `bundle_save_model`
- Load Bundle via `acl.mdl.bundle_load_from_file` and execute sub-models sequentially
