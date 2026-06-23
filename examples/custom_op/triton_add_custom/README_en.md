# Triton Custom Operator TensorFlow Graph Integration Sample

## Sample Overview

- Graph construction entry: `TensorFlow`
- Operator programming language: `Triton`
- Compilation method: `Pre-compiled to npubin`
- Model sinking capability: `Not applicable`
- Core pipeline: `Triton kernel -> TensorFlow deliverables + GE deliverables -> TensorFlow graph integration execution`
- Difference from other samples: This sample focuses on Triton kernel and TensorFlow/GE deliverables cooperation, not involving TorchAir, nor `ATC offline compilation -> om offline model` model sinking.

This sample demonstrates how to integrate Triton Add kernel into TensorFlow and complete NPU execution through GE custom operator deliverables. The pipeline includes `npubin` generation, TensorFlow custom operator so building, GE deliverables building, and final TensorFlow script result verification.

## Applicable Scenarios

- Want to see custom operator graph integration example in `TensorFlow + Triton` scenario.
- Want to reference cooperation between `npubin`, TensorFlow deliverables, and GE deliverables.
- Want to verify numerical correctness of Triton kernel in TensorFlow graph.
- Not suitable for understanding `TorchAir` or `ATC offline compilation -> om offline model` pipeline.

## Prerequisites

### CANN

- CANN toolkit and ops packages installed and configured.
- `ASCEND_HOME_PATH` set.
- Follow [installation guide](../../../docs/en/build.md#1-environment-preparation) to complete toolkit and ops package installation.

### Framework and Plugins

- Installed `Triton-Ascend`.
- Installed `TensorFlow 1.15` or `TensorFlow 2.6.5` and corresponding framework plugin packages.

References:

- [Triton-Ascend Installation Guide](https://gitcode.com/Ascend/triton-ascend/blob/main/docs/zh/installation_guide.md)
- [Triton-Ascend Quick Start](https://gitcode.com/Ascend/triton-ascend/blob/main/docs/zh/quick_start.md)
- [TensorFlow 1.15 Migration Guide](https://www.hiascend.com/document/detail/zh/TensorFlowCommunity/850/migration/tfmigr1/tfmigr1_000008.html)
- [TensorFlow 2.6.5 Migration Guide](https://www.hiascend.com/document/detail/zh/TensorFlowCommunity/850/migration/tfmigr2/tfmigr2_000007.html)

### Environment Variables

- `ASCEND_HOME_PATH`
- `ASCEND_CUSTOM_OPP_PATH`
- If need to customize `npubin` output directory, can set `TRITON_CACHE_DIR`

### Additional Dependencies

- `python3`
- `g++`
- `cmake`

## Quick Run

Execute shortest path in `examples/custom_op/triton_add_custom` directory:

### Recommended Method

```bash
cd add_custom_kernel
python3 add_custom_kernel.py

cd ../tensorflow
bash build_tf.sh

cd ../ge
bash run.sh
export ASCEND_CUSTOM_OPP_PATH="$(pwd)/build_out:$ASCEND_CUSTOM_OPP_PATH"

cd ../tensorflow
python3 ../script/run_add_custom_tf_1.15.py
```

Before execution, confirm `bin_path` in `ge/src/custom_op.cpp` can access `add_kernel.npubin` generated in step 1. If successful, terminal will print `The result of tf and ac is the same.`

### Step-by-step Method

1. First execute `python3 add_custom_kernel.py` in `add_custom_kernel/` to generate `npubin`.
2. Then execute `bash build_tf.sh` in `tensorflow/` to generate `outputs/libcustom_ops.so`.
3. Next execute `bash run.sh` in `ge/` to generate `build_out/libcust_opapi.so` and `framework/tensorflow/npu_supported_ops.json`.
4. Execute `export ASCEND_CUSTOM_OPP_PATH="$(pwd)/build_out:$ASCEND_CUSTOM_OPP_PATH"` to add `libcust_opapi.so` directory to environment variables.
5. Finally return to `tensorflow/` directory and execute `python3 ../script/run_add_custom_tf_1.15.py`.

If need to adjust `npubin` output location, can first set `TRITON_CACHE_DIR`; if need to re-specify `npubin` path, can modify `bin_path` in `ge/src/custom_op.cpp` according to actual cache location.

## Directory Structure and Key Files

```text
triton_add_custom
├── README.md
├── add_custom_kernel
│   └── add_custom_kernel.py        // Triton Add kernel implementation and compilation entry
├── ge
│   ├── CMakeLists.txt              // GE deliverables build script
│   ├── gen_npu_supported_ops_json.sh
│   ├── run.sh
│   └── src
│       └── custom_op.cpp           // Triton operator GE deliverables
├── tensorflow
│   ├── add_custom_triton_tf.cc     // TensorFlow-side custom operator declaration
│   └── build_tf.sh                 // TensorFlow deliverables build script
├── script
│   └── run_add_custom_tf_1.15.py   // TensorFlow test script
├── static_shape.png
└── dynamic_shape.png
```

Key files:

- `add_custom_kernel/add_custom_kernel.py`
  Triton Add kernel implementation and compilation entry.
- `tensorflow/add_custom_triton_tf.cc`
  TensorFlow-side custom operator declaration.
- `tensorflow/build_tf.sh`
  Compile `libcustom_ops.so`.
- `ge/src/custom_op.cpp`
  GE custom operator deliverables, implement loading `npubin` and initiating kernel execution.
- `ge/CMakeLists.txt`
  Build `libcust_opapi.so` and generate `npu_supported_ops.json`.
- `ge/run.sh`
  One-click script for GE substeps, responsible for configure, build, and install.
- `script/run_add_custom_tf_1.15.py`
  TensorFlow graph execution script, responsible for numerical consistency verification.

## Core Pipeline

1. `add_custom_kernel/add_custom_kernel.py` generates Triton kernel corresponding `npubin`.
2. `tensorflow/build_tf.sh` builds TensorFlow-side `libcustom_ops.so`.
3. `ge/CMakeLists.txt` builds GE deliverable `libcust_opapi.so`, and generates `npu_supported_ops.json` for TensorFlow use.
4. `script/run_add_custom_tf_1.15.py` loads TensorFlow-side so, and executes custom AddCustom operator through TensorFlow graph.

## Build Artifacts

- `tensorflow/outputs/libcustom_ops.so`
  TensorFlow-side custom operator deliverable.
- `ge/build_out/libcust_opapi.so`
  GE-side custom operator deliverable.
- `ge/build_out/framework/tensorflow/npu_supported_ops.json`
  Description file used by TensorFlow Adapter when loading custom operator support information.
- `add_kernel.npubin`
  Triton kernel compiled binary file, default written by Triton to cache directory, path can be controlled through `TRITON_CACHE_DIR`.

## Result Verification

When successful, can observe:

- `tensorflow/outputs/libcustom_ops.so` generated.
- `ge/build_out/libcust_opapi.so` generated.
- `ge/build_out/framework/tensorflow/npu_supported_ops.json` generated.
- Terminal output includes `The result of tf and ac is the same.`.

If failed, prioritize checking:

- Whether `bin_path` in `ge/src/custom_op.cpp` can correctly access generated `npubin`.
- Whether `ASCEND_CUSTOM_OPP_PATH` already includes `ge/build_out`.
- Whether `./outputs/libcustom_ops.so` can be accessed under test script startup directory.
- Whether TensorFlow, framework plugin package and Triton-Ascend installation match.

## Notes / Limitations

- Current sample depends on Triton-Ascend target hardware support scope, please refer to [Triton-Ascend project description](https://gitcode.com/Ascend/triton-ascend#hardware-support).
- `bin_path` in `ge/src/custom_op.cpp` needs to be consistent with actual generated `npubin` path.
- Test script defaults to loading TensorFlow-side so from `./outputs/libcustom_ops.so` in current working directory, recommend running test script in `tensorflow/` directory.
- Sample defaults to using static shape; if need dynamic shape, can enable `compile_dynamic_mode` configuration in test script.

## Appendix

### Dynamic / Static Shape Debug Switch

In `script/run_add_custom_tf_1.15.py` can enable dynamic shape through following configuration:

```python
custom_op.parameter_map["compile_dynamic_mode"].b = True
```

### Static Shape Dump Data Configuration

In `script/run_add_custom_tf_1.15.py` can enable dump data capability through following configuration:

```python
custom_op.parameter_map["enable_dump"].b = True
custom_op.parameter_map["dump_mode"].s = tf.compat.as_bytes("all")
custom_op.parameter_map["dump_path"].s = tf.compat.as_bytes("/home/test/output")
```

### Profiling Switch

```python
custom_op.parameter_map["profiling_mode"].b = True
custom_op.parameter_map["profiling_options"].s = tf.compat.as_bytes(
    '{"output":".","training_trace":"on","task_trace":"on","hccl":"on","aicpu":"on","aic_metrics":"PipeUtilization","msproftx":"off"}'
)
```

After exporting profiling data, can use following command to view result directory:

```bash
msprof --export=on --output=${profiling_path}
cd ${profiling_path}/mindstudio_profiler_output
```

Then find `msprof_*.json` and use tools like `chrome://tracing/` to load.

### Profiling Phenomenon Description

- Static shape result illustration: ![static_shape.png](./static_shape.png)
- Dynamic shape result illustration: ![dynamic_shape.png](./dynamic_shape.png)

Under static shape can observe `MODEL_EXECUTE`, `EVENT_WAIT`, `MEMCPY_ASYNC` and other stream synchronization and data transfer processes; under dynamic shape these phenomena will be different.
