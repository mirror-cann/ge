# ES API Generation Guide

This document explains how **Eager Style (ES) API** is generated in the current test framework, and where to modify when **ES API graph construction for other operators** is needed.

---

## I. How ES API is Currently Generated

### 1.1 Overall Process

ES API is generated using the **"operator prototype + generate_es_package.cmake function"** approach:

1. **Operator Prototype**: Add operator prototypes in `proto/test_ops.h` using `REG_OP(OpName)...OP_END_FACTORY_REG(OpName)`.
2. **Prototype Library**: Compile the above file into a dynamic library `test_ops_prot`, which registers all operators in GE when loaded.
3. **Code Generation**: CMake invokes the **gen_esb** tool during configuration/build to generate C/C++ ES graph construction APIs for each operator.
4. **Artifacts**: Generates `es_ge_test_ops.h`, `es_ge_test_ops_c.h`, and corresponding `es_<OpType>.cpp`, `es_<OpType>.h` for each operator, compiled into library `es_ge_test`.

Therefore: **Operators that can participate in ES API generation must first be registered with REG_OP in `proto/test_ops.h`.**

### 1.2 Key Components and Paths

| Component | Path/Description |
|------|------------|
| Operator Prototype Definition | `tests/framework/eager_style_graph_builder/proto/test_ops.h` (and `test_ops.cc` in the same directory) |
| Generation Script | `cmake/generate_es_package.cmake` (provides `add_es_library`) |
| This Directory's CMake | `tests/framework/eager_style_graph_builder/CMakeLists.txt` |
| Generated Artifacts Directory | Under build directory `eager_style_graph_builder/build/es_output/`, headers in `include/es_ge_test/` |

### 1.3 Generation Trigger Method (CMake)

In `tests/framework/eager_style_graph_builder/CMakeLists.txt`:

- Include the cmake function for generating operators via `include(generate_es_package.cmake)`.
- First define and compile the operator prototype library `test_ops_prot` (source file is `proto/test_ops.cc`, which includes REG_OP from `test_ops.h`).
- Then call:

```cmake
add_es_library(
    ES_LINKABLE_AND_ALL_TARGET es_ge_test
    OPP_PROTO_TARGET test_ops_prot
    OUTPUT_PATH ${ES_OUTPUT_PATH}
)
```

`add_es_library` will:

1. Derive the directory where operator prototypes are located from `test_ops_prot` output path
2. Set `ASCEND_OPP_PATH` during build and execute **gen_esb**
3. gen_esb loads operator libraries from that path and generates C/C++ code
4. Compile the generated code into `es_ge_test` dynamic library and install to `ES_OUTPUT_PATH`

Therefore: **The ES API operator set is entirely determined by "the operator prototypes loaded by gen_esb", which is currently the REG_OP included in `test_ops_prot`.**

### 1.4 Correspondence with gen_esb

- gen_esb input: The directory pointed to by environment variable `ASCEND_OPP_PATH` (i.e., the OPP path derived from `test_ops_prot`).
- gen_esb loads operator .so files from that path, reads operator metadata registered via REG_OP, and then for each operator generates:
  - C interface: `es_<OpType>.cpp` + declaration in `es_ge_test_ops_c.h`
  - C++ interface: `es_<OpType>.h`, aggregated in `es_ge_test_ops.h`

---

## II. Where to Modify When Needing ES API Graph Construction for Other Operators

If you want to use **more operators'** ES API for graph construction, just ensure these operators are registered in the "operator prototype library" when gen_esb runs. Currently, this library is `test_ops_prot` built from `proto/` in this directory.

### 2.1 Adding New Operators

**Modification Location:** `tests/framework/eager_style_graph_builder/proto/test_ops.h` (or in conjunction with `proto/test_ops.cc`)

**Operation:** Add a line in `test_ops.h` following the existing format: `REG_OP(NewOpName)...OP_END_FACTORY_REG(NewOpName)`, defining inputs/outputs/attributes (refer to existing operator prototypes in the same file).

- If header file alone is sufficient for registration, only modify `test_ops.h`.
- If the operator already has a REG_OP definition in other modules, you can also `#include` the corresponding header file in `test_ops.cc`, ensuring it's linked into `test_ops_prot.so`.

After saving and rebuilding, gen_esb will generate corresponding ES operators and include them in `es_ge_test_ops.h` / `es_ge_test_ops_c.h`. In test cases, you can then:

- Use C++: `#include "es_ge_test_ops.h"`, call `es::NewOpName(...)`, etc.
- Use C: `#include "es_ge_test_ops_c.h"`, call generated C functions.

For more `add_es_library` parameters and behavior, see comments inside `cmake/generate_es_package.cmake`.
