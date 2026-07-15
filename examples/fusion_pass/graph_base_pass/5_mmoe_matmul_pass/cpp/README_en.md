# Sample Usage Guide

## Function Description

This sample is a MMOE network first-layer MatMul fusion custom pass, registered via `REGISTER_CUSTOM_PASS` (graph_base_pass).

For multiple MatMul operators in the first layer of the MMOE network with "same-source input, same weight shape", it merges them into a single large MatMul, then splits back via SplitV along the original output dimension, reducing MatMul operator count and improving execution efficiency.

### Differences from pattern_base_pass Version

| Feature | pattern_base_pass Version | graph_base_pass Version (this sample) |
|---------|---------------------------|---------------------------------------|
| Registration | REG_FUSION_PASS + FusionBasePass | REGISTER_CUSTOM_PASS + CustomPassFn |
| Merged nodes | Fixed 2-node merge | Supports full N-node merge |
| Dependencies | Requires gen_es_api, es_all library | Only depends on graph, register libraries |
| Graph modification API | SubgraphRewriter + es API | Graph/GNode native API |

### Mathematical Equivalence

Original structure (N independent MatMul):

```text
y1 = x @ w1     [m, k] @ [k, n] = [m, n]
y2 = x @ w2     [m, k] @ [k, n] = [m, n]
...
yN = x @ wN     [m, k] @ [k, n] = [m, n]
```

Fused structure (1 large MatMul + SplitV):

```text
W  = ConcatV2D(w1, w2, ..., wN, axis=const_idx)   [k, n*N]
Y  = x @ W                                        [m, k] @ [k, n*N] = [m, n*N]
(y1, y2, ..., yN) = SplitV(Y, size_splits=[n, n, ..., n], split_dim=const_idx)
```

Matrix multiplication is distributive over column concatenation: `x @ [w1 | w2 | ... | wN] = [x@w1 | x@w2 | ... | x@wN]`, so each output after SplitV is mathematically equivalent to the original MatMul output.

## Directory Structure

```text
├── src
│   └── mmoe_matmul_pass.cpp    // Pass implementation file
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

1. **FindNodesCanFusion**: Traverse all nodes in the graph, collect MatMulV2 and MatMulV3 nodes.

2. **FindConstIdx**: For each MatMul, determine which input is a Const weight (const_idx) and which is a data input (data_idx).

3. **FindRootNode**: Recursively search upward from the MatMul data input to find the root (through Reshape/Squeeze and other intermediate nodes), returning the root node and root_output_idx.

4. **FindMatmulNodes**: From the root, search downstream along the Reshape/Squeeze chain to find all same-source MatMul satisfying the following conditions:
   - const_idx is consistent with the base
   - transpose_x1 / transpose_x2 is consistent with the base
   - data input shape is consistent with the base
   - combined_dim (weight dimension along const_idx axis) is consistent with the base

5. **ReplaceMmNodes**: For each matched group, merge:
   - `concat_matrices`: Concatenate each Const weight along const_idx axis via ConcatV2D
   - Create new MatMulV2: data input + concatenated weight, inheriting transpose attributes
   - Create new SplitV: num_split=N, size_splits=[combined_dim, ...], split_dim=const_idx
   - Reconnect each original MatMul downstream to corresponding SplitV output
   - Delete original MatMul nodes, mark merged nodes

## Program Compilation

Assume the CANN software package installation directory is INSTALL_PATH, for example `/home/HwHiAiUser/Ascend/`.

1. Configure environment variables.

   Run the environment variable script in the software package with the following command:

```bash
   source ${ASCEND_PATH}/set_env.sh
```

   `${ASCEND_PATH}` is the cann path under the CANN software package installation directory. Replace with the actual installation path of the relevant software package, for example `${INSTALL_PATH}/cann`.

2. Execute the following command to compile. After compilation, the dynamic library file **libmmoe_matmul_pass.so** is generated in the **build** directory.

   Execute sequentially:

```bash
   mkdir build && cd build
   cmake .. && make
```

3. After successful compilation, install the dynamic library file libmmoe_matmul_pass.so to the custom fusion pass directory via `make install`.

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
     atc --model=./model.onnx --framework=5 --soc_version=Ascend910B3 --output=./model_fused
```

     After execution, the **model_fused.om** model file is generated in the **data** directory.

   - Check execution results:

     - When the custom pass takes effect, comparing the intermediate dump graphs during NPU compilation, the model has been optimized as expected:
       - Before fusion (PreRunBegin graph): N MatMul nodes exist
       - After fusion (RunCustomPass_AfterInferShape graph): 1 large MatMul + 1 SplitV + 1 ConcatV2D exist

     - The following output appears in the logs:

```text
       MmoeMatmulPass begin.
       MmoeMatmulPass: found 4 MatMul nodes.
       MmoeMatmulPass: trying to merge 4 nodes from root.
       MmoeMatmulPass: merged 4 MatMul nodes.
       MmoeMatmulPass end.
```

3. Use the one-click verification script.

```bash
   cd data
   ./quick_verify.sh
```

   The script automatically completes compilation, ONNX generation, ATC compilation, and dump graph verification.
