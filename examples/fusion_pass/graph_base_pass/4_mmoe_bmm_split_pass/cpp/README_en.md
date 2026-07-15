# Sample Usage Guide

## Function Description

This sample is a MMOE network MatMul fusion custom pass, registered via `REGISTER_CUSTOM_PASS` (graph_base_pass).

For multiple MatMul operators in non-first layers of the MMOE network with "same-source input, same weight shape", it merges inputs via Pack operator along the first axis to shape `[N, ...]`, performs batch matrix multiplication via BatchMatMulV2, then restores original output shapes via Split + Squeeze, reducing MatMul operator call count and improving execution efficiency.

### Differences from pattern_base_pass Version

| Feature | pattern_base_pass Version | graph_base_pass Version (this sample) |
|---------|---------------------------|---------------------------------------|
| Registration | REG_FUSION_PASS + PatternFusionPass | REGISTER_CUSTOM_PASS + CustomPassFn |
| Merged nodes | Fixed 2-node merge | Supports full N-node merge |
| Dependencies | Requires gen_es_api, es_all library | Only depends on graph, register libraries |
| Graph modification API | SubgraphRewriter + es API | Graph/GNode native API |

### Mathematical Equivalence

Original structure (N independent MatMul):

```text
y1 = x1 @ w1     [m, k] @ [k, n] = [m, n]
y2 = x2 @ w2     [m, k] @ [k, n] = [m, n]
...
yN = xN @ wN     [m, k] @ [k, n] = [m, n]
```

Fused structure (Pack + BatchMatMul + Split + Squeeze):

```text
X  = Pack(x1, x2, ..., xN, axis=0)     [N, m, k]
W  = Pack(w1, w2, ..., wN, axis=0)     [N, k, n]
Y  = BatchMatMulV2(X, W)               [N, m, k] @ [N, k, n] = [N, m, n]
(y1, y2, ..., yN) = Split(Y, num_split=N, axis=0)
yi = Squeeze(yi_raw, axis=0)           [m, n]
```

Pack stacks along the first axis without changing matrix multiplication results. Split + Squeeze splits along the first axis to restore each output, so each output is mathematically equivalent to the original MatMul output.

## Directory Structure

```text
├── src
│   └── mmoe_bmm_split_pass.cpp    // Pass implementation file
├── CMakeLists.txt               // Build script
├── data
│   ├── gen_onnx.py             // Generate MMOE-style ONNX model
│   └── quick_verify.sh         // One-click verification script
```

## Environment Requirements

- Compiler: GCC >= 7.3.x
- CANN version >= 8.5.0 (REGISTER_CUSTOM_PASS supported since 8.5.0)
- Python >= 3.7, depends on onnx, numpy
- [Environment preparation](../../../../../docs/zh/build.md) completed.

## Implementation Steps

1. **FindSameLevelMatmulNode**: Traverse all nodes in the graph, collect MatMulV2 and MatMulV3 nodes, recursively search upward for root nodes (Data nodes or nodes with multiple non-const inputs), group by root name + matmul level.

2. **CheckMatmulShape**: Verify that all MatMul nodes in the group have consistent transpose_x1/transpose_x2 attributes and input shapes.

3. **ProcessPackNode**: For each matched group, create Pack operators for both inputs, merging along axis=0 to `[N, ...]`.

4. **ProcessBatchMatmulNode**: Create BatchMatMulV2, inheriting transpose attributes, connecting Pack outputs.

5. **ProcessSplitAndSqueezeNode**: Create Split (axis=0) + Squeeze (axis=0), reconnect each original MatMul downstream to corresponding Squeeze output, delete original MatMul nodes.

## Program Compilation

Assume the CANN software package installation directory is INSTALL_PATH, for example `/home/HwHiAiUser/Ascend/`.

1. Configure environment variables.

   Run the environment variable script in the software package with the following command:

```bash
   source ${ASCEND_PATH}/set_env.sh
```

   `${ASCEND_PATH}` is the cann path under the CANN software package installation directory. Replace with the actual installation path of the relevant software package, for example `${INSTALL_PATH}/cann`.

2. Execute the following command to compile. After compilation, the dynamic library file **libmmoe_bmm_split_pass.so** is generated in the **build** directory.

   Execute sequentially:

```bash
   mkdir build && cd build
   cmake .. && make
```

3. After successful compilation, install the dynamic library file libmmoe_bmm_split_pass.so to the custom fusion pass directory via `make install`.

```bash
   make install
```

   After sample verification is complete, execute the following command to clean the custom pass so installed in the CANN package to avoid affecting subsequent UT/ST:

```bash
   make clean_custom_pass
```

## Program Execution

1. Configure environment variables (skip if already executed).

```bash
   source ${ASCEND_PATH}/set_env.sh
```

2. Use ATC for offline inference.

   - Set environment variables to dump the model graph during compilation:

```bash
     export DUMP_GE_GRAPH=1
```

   - Execute the ONNX model generation script in the **data** directory:

```bash
     python3 gen_onnx.py --experts 4
```

     After execution, the ONNX model file **model.onnx** is generated in the **data** directory.

   - Execute the ATC command, where soc_version should be filled according to the actual model runtime environment:

```bash
     atc --model=./model.onnx --framework=5 --soc_version=Ascend910_9362 --output=./model_fused
```

     After execution, the **model_fused.om** model file is generated in the **data** directory.

   - Check execution results:

     - When the custom pass takes effect, comparing the intermediate dump graphs during NPU compilation, the model has been optimized as expected:
       - Before fusion (PreRunBegin graph): N*2 MatMul nodes exist (first layer N + non-first layer N)
       - After fusion (RunCustomPass_AfterInferShape graph): first layer N MatMul nodes retained, non-first layer N MatMul nodes merged into Pack + BatchMatMulV2 + Split + Squeeze

     - The following output appears in the logs:

```text
       MmoeBmmSplitPass begin.
       MmoeBmmSplitPass: merged 1 groups.
       MmoeBmmSplitPass end.
```

3. Use the one-click verification script.

```bash
   cd data
   ./quick_verify.sh
```

   The script automatically completes compilation, ONNX generation, ATC compilation, and dump graph verification.
