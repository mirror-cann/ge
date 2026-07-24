# ConvTransFormatPass Python Example Usage Guide

This directory provides a **pure Python** version example of `graph_base_pass/3_modify_conv_data_format_pass`, main flow identical to C++ `ConvTransFormatPass`:

- Traverse `Conv2D` / `Conv2DV2` in graph, filter nodes with `data_format == NCHW`;
- Change `data_format` to `NHWC`;
- BFS from the Conv output and match `Transpose` nodes with `perm == [0,2,3,1]` and `[0,3,1,2]` in order;
- Call `can_fuse` before the rewrite, then call `report_fuse` after reconnecting edges and before deleting old nodes;
- Delete the matching `Transpose` and perm constant nodes.

This example inherits `FusionBasePass` and overrides `run()`. It performs the rewrite through `Graph.remove_edge` / `add_data_edge` / `remove_node` and `Node.set_attr`. Because it **does not use** `SubgraphRewriter`, it calls `can_fuse` and `report_fuse` explicitly.

## Differences from C++ Version

1. **Full Graph Rollback**: C++ uses backup graph to restore when `SetAttr` or edge deletion fails. Current Python `Graph` does not support full graph deep copy. This example throws exception on failure, **does not guarantee** same atomic rollback semantics as C++.
2. **Reading Transpose perm**: C++ uses `GNode::GetInputConstData`. Python side reads via `value` attribute of `Const` / `Constant` node at perm input (same as Const validation in `pattern_base_pass/4_add_zero_pass`). If perm in graph does not appear in this form, may not recognize and delete `Transpose`, coverage may differ slightly from C++.

## Directory Structure

```tree
python/
├── README.md                     // Python example description
├── CMakeLists.txt                // Build script for generating es_all Python ES API
├── src
│   ├── python_modify_conv_data_format_pass.py // Python pass implementation file
```

## Prerequisites

- Completed CANN environment variable setup via `source ${ASCEND_PATH}/set_env.sh`. For more guidance, refer to [C++ Example README](../cpp/README.md) environment variable configuration step
- Can import GE Python package (contains `ge.graph`, `ge.passes` and pass loading chain)

Python pass runtime loads precompiled binary components based on `pybind11`. CANN package prioritizes providing artifacts matching current Python version; if no matching artifact, automatically enters fallback compilation flow. Fallback compilation requires `pybind11` installed in current Python environment.

## Usage

1. Let GE load this Python pass during compilation via environment variable (when in `3_modify_conv_data_format_pass` directory):

    ```bash
    export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/python_modify_conv_data_format_pass.py
    ```

2. Refer to [C++ Example README](../cpp/README.md) program execution chapter for validation.

## Expected Result

Similar log output:

```text
PythonConvTransFormatPass is starting
Remove output edges success
Remove output edges success
PythonConvTransFormatPass completed
```

When comparing pbtxt exported by `DUMP_GE_GRAPH`, should see Conv `data_format` is `NHWC` and target `Transpose` removed (consistent with C++ example description).
