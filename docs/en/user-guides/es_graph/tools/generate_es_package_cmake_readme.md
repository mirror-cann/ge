# add_es_library Usage Guide

## Overview

Provides the following CMake functions for generating Eager Style (ES) artifacts:

- **`add_es_library_and_whl`**: Generate C/C++ dynamic library + Python wheel package (complete artifacts)
- **`add_es_library`**: Only generate C/C++ dynamic library (for pure C/C++ projects)

## Quick Start

### Prerequisites
1. Properly install `toolkit` package via [Installation Guide](../../../quick_install.md), and **correctly configure environment variables** as instructed

2. Define prototype dynamic library
```cmake
add_library(opgraph_math SHARED #Required to be .so form
)
```

### 1. Import Functions

#### Using find_package

```cmake
# Add module path in your CMakeLists.txt (ASCEND_HOME_PATH comes from environment variable configuration in Prerequisites)
list(APPEND CMAKE_MODULE_PATH "${ASCEND_HOME_PATH}/include/ge/cmake")

# Find module
find_package(GenerateEsPackage REQUIRED)
```

**Note**:
- Current version requires manually adding `CMAKE_MODULE_PATH` in CMakeLists.txt
- **Planned support**: Future versions will support direct `find_package` after just `source set_env.sh` (no need to manually add path)

### 2. Generate ES API

#### Generate Complete Artifacts

```cmake
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math #Prototype library target from Prerequisites
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)
```

#### Only Generate C/C++ Library

```cmake
add_es_library(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)
```

### 3. Use Generated Artifacts

```cmake
# Link in your application
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE es_math)
```

### 4. Build

```bash
# Just build your application directly
make my_app
# → Will automatically trigger ES package build
```

## Parameter Description

### Function Parameters

Both functions have identical parameters:

| Parameter | Required | Description | Example |
|------|--------|------|------|
| `ES_LINKABLE_AND_ALL_TARGET` | ✅ Required | Library target name exposed externally | `es_math`, `es_nn`, `es_cv` |
| `OPP_PROTO_TARGET` | ✅ Required | Operator prototype library's CMake target name (supports SHARED/STATIC/INTERFACE/OBJECT types) | `opgraph_math`, `opgraph_nn` |
| `OUTPUT_PATH` | ☑️ Optional  | Root directory for artifact output. If not specified, artifacts remain in build directory | `${CMAKE_BINARY_DIR}/output` |
| `EXCLUDE_OPS` | ☑️ Optional  | Operators to exclude from generation | `Add,Conv2D` |

**Difference**:
- `add_es_library_and_whl`: Generate library + wheel package
- `add_es_library`: Only generate library (skip wheel package generation)

**Important**:
- Since ES artifacts are dynamically generated inside the function, and cmake overall handles configuration and build in separate phases, our function internally has reconfiguration and build operations, which is why we currently directly provide an interface type `ES_LINKABLE_AND_ALL_TARGET`
- The function automatically derives prototype library path from `OPP_PROTO_TARGET`'s `LIBRARY_OUTPUT_DIRECTORY`, which is the basic condition for generating ES artifacts corresponding to the prototype library

### Historical Prototype Library Related CMake Variables

Set the following variables via `set()` before calling the function to control historical prototype library functionality:

| Variable | Type | Description | Example |
|------|------|------|------|
| `GE_ES_EXTRACT_HISTORY` | bool (optional) | When ON, passes `--es_mode=extract_history` to gen_esb, enabling **historical prototype library generation mode** (archive current prototype as JSON, for subsequent version code generation); if not set or OFF, gen_esb defaults to `codegen` (only generate C++ API) | `set(GE_ES_EXTRACT_HISTORY ON)` |
| `GE_ES_RELEASE_VERSION` | Optional | **Current new version number**, archive step will archive current prototype as this version; must be set in historical prototype library generation mode | `set(GE_ES_RELEASE_VERSION "8.0.RC1")` |
| `GE_ES_RELEASE_DATE` | Optional | Specify release date in historical prototype library generation mode (format `YYYY-MM-DD`), if not set gen_esb uses current date | `set(GE_ES_RELEASE_DATE "2026-02-28")` |
| `GE_ES_BRANCH_NAME` | Optional | Specify release branch name in historical prototype library generation mode. **Note**: When set to `master`, function ignores all archive-related variables (`GE_ES_EXTRACT_HISTORY`/`GE_ES_RELEASE_VERSION`/`GE_ES_RELEASE_DATE`), only runs pure code generation mode | `set(GE_ES_BRANCH_NAME "release/8.0")` |

> **Historical Prototype Library Path Auto-Derivation**: Function internally auto-derives `${CANN_INSTALL_PATH}/cann/opp/history_registry/<module>` path based on cmake file installation location. When path exists and is non-empty, automatically passes `--history_registry` to gen_esb, **caller doesn't need to explicitly set historical prototype library path**.
>
> **Complete Commercial Release Mode**: When `GE_ES_EXTRACT_HISTORY=ON` and ops package contains historical prototype library, `add_es_library` internally automatically executes gen_esb twice in sequence (code generation + historical prototype library archive&merge), **caller doesn't need extra handling**. One `add_es_library` call completes all operations needed for complete commercial release (see Example 11).

## Output Artifacts

### Artifacts Generated by add_es_library_and_whl

```
OUTPUT_PATH/
├── include/
│   └── es_math/               # Header file directory
│       ├── es_math_ops.h      # C++ interface aggregated header file
│       ├── es_math_ops_c.h    # C interface aggregated header file
│       └── es_add.h ...       # Single operator header file (generally multiple files)
├── lib64/
│   ├── libes_math.so          # Dynamic library
│   └── libes_math.a           # Static library
└── whl/
    └── es_math-1.0.0-py3-none-any.whl  # Python package
```

### Artifacts Generated by add_es_library

```
OUTPUT_PATH/
├── include/
│   └── es_math/               # Header file directory
│       ├── es_math_ops.h      # C++ interface aggregated header file
│       ├── es_math_ops_c.h    # C interface aggregated header file
│       └── es_add.h ...       # Single operator header file (generally multiple files)
└── lib64/
    ├── libes_math.so          # Dynamic library
    └── libes_math.a           # Static library
```

**Note**:
- **Aggregation meaning**: Contains graph building API for all operators under es_math
- **Dynamic and Static Libraries**: Each generation produces both `lib<name>.so` and `lib<name>.a`. External interface target (like `es_math`) defaults to linking dynamic library; if static linking is needed, manually specify `lib64/lib<name>.a` or link static library via `target_link_libraries(your_target PRIVATE ${OUTPUT_PATH}/lib64/libes_math.a)` etc.

### Additional Artifacts After Enabling Historical Prototype Library Related Modes

#### Historical Prototype Library Generation Mode (`GE_ES_EXTRACT_HISTORY=ON`)

gen_esb directly outputs to `OUTPUT_PATH`, generating the following JSON structure:

```
OUTPUT_PATH/
├── index.json                          # Version index (list of all archived versions)
└── registry/
    └── <GE_ES_RELEASE_VERSION>/
        ├── metadata.json               # Version metadata (version number, release date, branch name etc.)
        └── operators.json              # Operator prototype data (IR structured description)
```

#### Code Generation Mode with Historical Compatibility (Auto-detected Historical Prototype Library)

C++ header files will have multi-version overload signatures for the same operator (historical version signature + current version signature), `.so` contains implementations of all versions simultaneously, forward compatible with old version calling methods. Artifact directory structure is same as standard mode, only header file content differs.

## Generated Targets

### Targets for External Use

| Target Name | Purpose | Description |
|------------|------|------|
| `es_math` | **Link Dependency** | **Users link through this target, automatically triggers build**; defaults to linking dynamic library (.so), static library (.a) also generated simultaneously. If `OUTPUT_PATH` specified, can directly get static library file from `lib64/`; if not specified, remains in CMake default output directory. |

## Usage Examples

### Example 1: Basic Usage

```cmake
# 1. Define prototype dynamic library (Prerequisites)
add_library(opgraph_math SHARED
)
# 2. Generate ES API package (library + wheel package)
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)

# 3. Use in application
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE es_math)
```
> **Tip**: If project has OBJECT/STATIC intermediate targets that need ES header files during compilation phase, can additionally add `add_dependencies(<intermediate_target> build_es_math)` (`build_es_***` name changes with package name), to ensure code generation completes before compiling that target. Final executable or shared library just needs `target_link_libraries(... es_math)` to automatically trigger related dependencies.

Supplementary Example: Including Object Library

```cmake
add_library(my_obj OBJECT foo.cc bar.cc)
# Ensure es_math header files generated before compiling object (recommended to add explicitly)
add_dependencies(my_obj build_es_math)
# Pass header file search path effect
target_link_libraries(my_obj PRIVATE es_math)

add_executable(my_app $<TARGET_OBJECTS:my_obj>)
target_link_libraries(my_app PRIVATE
        es_math
)
```

### Example 2: Only Generate C/C++ Library

```cmake
# Only generate C/C++ library (skip Python wheel package)
add_es_library(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)
```

### Example 3: Exclude Partial Operator Generation

```cmake
# Generate math TARGET
# Generated artifacts won't contain es_Add_c.h; es_Add.h, es_Add.cpp, es_Add.py
add_es_library(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
    EXCLUDE_OPS       Add
)
```

### Example 4: Using OBJECT Type Prototype Library

```cmake
# Define OBJECT type prototype library
add_library(opgraph_math OBJECT
    src/math_op1.cpp
    src/math_op2.cpp
)

# Generate ES API (function internally automatically creates temporary SHARED library wrapper)
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math  # OBJECT type
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)
```

### Example 5: Not Specifying OUTPUT_PATH

```cmake
# Not specifying OUTPUT_PATH, artifacts will remain in build directory
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    # OUTPUT_PATH not specified
)
```

### Artifacts Display

Build artifacts remain in build directory and CMake default target output directory, won't be additionally organized into unified `include/`, `lib64/`, `whl/` directories.

```
<build_dir>/es_math_build/
├── generated_code/            # gen_esb and wrapper output
│   ├── es_math_ops.h          # C++ interface aggregated header file
│   ├── es_math_ops_c.h        # C interface aggregated header file
│   ├── es_add.h ...           # Single operator header file (generally multiple files)
│   ├── es_add.cpp ...         # Single operator implementation file (generally multiple files)
│   └── es_math_all_in_one.cpp # Aggregated wrapper
├── python_package/
│   └── dist/
│       └── es_math-1.0.0-py3-none-any.whl   # Python package
├── libes_math.so              # Dynamic library (CMake default library output directory)
└── libes_math.a               # Static library
```

### Example 6: Multiple TARGETs

```cmake
# Generate math TARGET
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)

# Generate nn TARGET
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_nn
    OPP_PROTO_TARGET  opgraph_nn
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)

# Use multiple TARGETs
add_executable(my_inference_app main.cpp)
target_link_libraries(my_inference_app PRIVATE
    es_math
    es_nn
)
```

### Example 7: Use in C++ Code

```cpp
#include "es_math/es_math_ops.h"
#include "ge/es/graph_builder.h"

using namespace ge::es;

int main() {
    EsGraphBuilder builder("my_graph");

    // Create inputs
    auto input1 = builder.CreateConstFloat(1.0f);
    auto input2 = builder.CreateConstFloat(2.0f);

    // Use generated ES API
    auto result = Add(input1, input2);

    auto graph = builder.build_and_reset();
    return 0;
}
```

### Example 8: Use in Python

```bash
# Install wheel package
pip install output/whl/es_math-1.0.0-py3-none-any.whl
```

```python
# Plugins auto-loaded via entry_point mechanism, use unified import method
from ge.es.math import Add
from ge.es import GraphBuilder

builder = GraphBuilder("my_graph")
input1 = builder.create_const_float(1.0)
input2 = builder.create_const_float(2.0)

result = Add(input1, input2)
graph = builder.build_and_reset()
```

**Note**:
- Plugins auto-loaded via entry_point mechanism, import path is `ge.es.<module_name>`
- `module_name` is derived by removing `es_` prefix from `ES_LINKABLE_AND_ALL_TARGET` (like `es_math` → `math`)
- Can use `ge.es.list_plugins()` to view all loaded plugin names
- Can use `ge.es.get_plugin('math')` to check if plugin exists (returns module object or None).

### Example 9: Historical Prototype Library Generation Mode (Commercial Release First Build, Archive Current Version Prototype)

```cmake
# Extract IR prototype from installed operator prototype .so, archive as JSON for next commercial release use
set(GE_ES_EXTRACT_HISTORY ON)
set(GE_ES_RELEASE_VERSION "8.0.RC1")
set(GE_ES_RELEASE_DATE    "2026-02-28")  # Optional, if not set uses current date
set(GE_ES_BRANCH_NAME     "release/8.0") # Optional; if set to master won't execute archive, only code generation

add_es_library(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)
# Artifacts: output/index.json, output/registry/8.0.RC1/metadata.json, output/registry/8.0.RC1/operators.json
```

### Example 10: Code Generation Mode with Historical Compatibility (Auto-consume Existing Historical Data, Generate C++ API with Overloads)

```cmake
# Function internally auto-detects ${CANN_INSTALL_PATH}/cann/opp/history_registry/math,
# when path exists and is non-empty, auto-generates C++ interfaces with historical compatible overload signatures, no need to manually set path
set(GE_ES_RELEASE_VERSION  "8.0.RC2")

add_es_library(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)
# Generated header files have multi-version overloads for same operator (old signature + new signature), forward compatible with old version calls
```

### Example 11: Complete Commercial Release Mode (Single Call, Function Internally Auto-completes Code Generation + Historical Prototype Library Archive&Merge)

```cmake
# GE_ES_EXTRACT_HISTORY=ON + ops package contains historical prototype library (auto-detected) = complete commercial release mode
# add_es_library internally auto executes gen_esb twice in sequence, caller doesn't need extra handling:
#   gen_esb call 1: default codegen mode, auto-selects historical versions within window anchored at current date → compares current prototype → generates C++ API with overloads
#   gen_esb call 2: --es_mode=extract_history, copies existing historical library from _AUTO_HISTORY_REGISTRY to OUTPUT_PATH, appends current version prototype in OUTPUT_PATH → OUTPUT_PATH contains complete historical prototype library
set(GE_ES_EXTRACT_HISTORY  ON)
set(GE_ES_RELEASE_VERSION  "8.0.RC2")     # Current new version number, for archive step
set(GE_ES_RELEASE_DATE     "2026-02-28")  # Optional
set(GE_ES_BRANCH_NAME      "develop") # Optional; if set to master won't archive, only code generation

add_es_library(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)
```

## Naming Rules

### Artifact Naming

| Artifact Type | Naming Rule | Example (ES_LINKABLE_AND_ALL_TARGET=es_math) |
|---------|---------|--------------------------------|
| Dynamic Library | `lib<ES_LINKABLE_AND_ALL_TARGET>.so` | `libes_math.so` |
| Static Library | `lib<ES_LINKABLE_AND_ALL_TARGET>.a` | `libes_math.a` |
| Python Package | `<ES_LINKABLE_AND_ALL_TARGET>-1.0.0-py3-none-any.whl` | `es_math-1.0.0-py3-none-any.whl` |
| Aggregated Header File | `es_<name>_ops.h` | `es_math_ops.h` |


### Target Naming

| Target Type | Naming Rule | Example (ES_LINKABLE_AND_ALL_TARGET=es_math) |
|------------|---------|--------------------------------|
| External Library | `<ES_LINKABLE_AND_ALL_TARGET>` | `es_math` |

## Notes

1. **ES_LINKABLE_AND_ALL_TARGET Naming**:
   - Recommend using `es_` prefix (like `es_math`, `es_nn`)
   - Use lowercase letters and underscores
   - Avoid special characters and C++ keywords

2. **Historical Prototype Library Related Variables**:
   - Historical prototype library path is **auto-derived** by function (`${CANN_INSTALL_PATH}/cann/opp/history_registry/<module>`), when path exists and is non-empty auto-enables, caller doesn't need to pass parameters
   - **master branch**: When `GE_ES_BRANCH_NAME` is `master`, will ignore `GE_ES_EXTRACT_HISTORY`/`GE_ES_RELEASE_VERSION`/`GE_ES_RELEASE_DATE`, only run pure code generation mode (historical prototype library path still passed, for generating C++ API with overloads)
   - `GE_ES_EXTRACT_HISTORY=ON` and ops package contains historical prototype library (auto-detected) is **complete commercial release mode**, function internally auto executes gen_esb twice (code generation + historical prototype library archive&merge), caller doesn't need any extra handling (see Example 11)
   - **Version Deduplication**: If historical prototype library's `index.json` already contains same version number as `GE_ES_RELEASE_VERSION`, skips archive step, only executes code generation (avoid duplicate archiving); simultaneously copies complete historical prototype library from CANN installation path to `OUTPUT_PATH`, convenient for subsequent path change installation
   - Pure historical archive mode (only set `GE_ES_EXTRACT_HISTORY=ON`, ops package has no historical prototype library): gen_esb outputs to `OUTPUT_PATH`, only generates JSON, no C++ API
   - In complete commercial release mode, code generation step gen_esb doesn't pass `--release_version`, auto-selects historical versions within window anchored at current date for comparison; archive step uses `GE_ES_RELEASE_VERSION` as new version number to write into historical library
   - In complete commercial release mode, archive step copies existing historical library from `_AUTO_HISTORY_REGISTRY` (CANN installation path, read-only) to `OUTPUT_PATH` (build directory, writable), gen_esb appends current version entry in `OUTPUT_PATH`; first build (no existing historical library in CANN path) directly outputs to `OUTPUT_PATH`; final `OUTPUT_PATH` contains complete merged result of all historical versions + current version
   - `GE_ES_RELEASE_VERSION` must be set during archiving, otherwise archived version cannot be identified
   - Historical prototype library data (JSON files) usually installed with ops package, default location `${CANN_INSTALL_PATH}/cann/opp/history_registry/<package_name>/`


## Dependency Requirements

- **CMake Version**: >= 3.16
- **CANN run Package**: Installed and environment variables set (or manually specify path)
- **gen_esb**: This is internal es api code generation binary tool, integrated in run package, can view usage via `gen_esb --help`
- **OPP_PROTO_TARGET**: Must exist
- **Python3**: >= 3.7 (only needed for `add_es_library_and_whl`)
- **Python Packages**: setuptools, wheel (only needed for `add_es_library_and_whl`), recommend using setuptools==59.6.0、wheel==0.37.1 compatible version or higher version

## Common Questions

### Q: How can multiple TARGETs share output directory?

A: All TARGETs can use same `OUTPUT_PATH`, header files will be organized in subdirectories by package name:

```cmake
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output  # Shared path
)

add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_nn
    OPP_PROTO_TARGET  opgraph_nn
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output  # Shared path
)

# Output structure:
# output/
# ├── include/
# │   ├── es_math/
# │   └── es_nn/
# ├── lib64/
# │   ├── libes_math.so, libes_math.a
# │   └── libes_nn.so, libes_nn.a
# └── whl/
#     ├── es_math-1.0.0-py3-none-any.whl
#     └── es_nn-1.0.0-py3-none-any.whl
```

### Q: What if gen_esb cannot be found?

A: Function will automatically search for gen_esb, if fails please check:

1. Confirm CANN run package is installed
2. Execute `source ${ASCEND_HOME_PATH}/set_env.sh` or set correct CMAKE_MODULE_PATH
3. gen_esb will auto-derive from PATH environment variable or cmake file path

Function will output detailed detection information, troubleshoot based on logs.

## Complete Example

```cmake
# ===== Basic Settings =====
cmake_minimum_required(VERSION 3.16)
project(my_es_project LANGUAGES CXX)

# ===== Import Functions (Recommended: use find_package) =====
list(APPEND CMAKE_MODULE_PATH "${ASCEND_HOME_PATH}/include/ge/cmake")
find_package(GenerateEsPackage REQUIRED)

# ===== Part 1: Prerequisites - Define Prototype Library =====
add_library(opgraph_math SHARED
)

# ===== Part 2: Generate ES API Package =====
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)

# ===== Part 3: Use in Application =====
add_executable(my_app
    src/main.cpp
    src/inference.cpp
)

# Link ES_LINKABLE_AND_ALL_TARGET
target_link_libraries(my_app PRIVATE
    es_math  # Auto get dependencies, headers and library
)
```

**Build Command**:

```bash
# Just one command!
make my_app
```
