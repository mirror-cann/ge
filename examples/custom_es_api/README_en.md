# Introduction

Since ES graph construction API is generated based on IR prototypes, some operator IRs cannot support all information expression (Conv) or IR expressions have redundant information (IdentityN), so it is recommended that users encapsulate ES API themselves while supporting user-defined behaviors.<br>
This Sample aims to guide users on how to customize ES API <br>
**Note:** For operators like Conv2D, there will be Format issues when using the default generated API directly, so you need to add logic yourself and generate custom ES API (see [Sample 1](#sample1))

## Quick Start

1. Follow the [installation guide](../../docs/zh/quick_install.md) to correctly install `toolkit` and `ops` packages, and properly configure environment variables
2. Run the script via command `bash run_sample.sh` [run_sample.sh](run_sample.sh)

## Expected Results

Generate pbtxt format dump graph in the project build directory

## Implementation Steps

### Step 1. [Optional] Generate ES API Code Reference

Generate ES_API code via gen_esb executable program (refer to [gen_esb_readme.md](../../docs/en/user_guides/es_graph/tools/gen_esb.md))

### Step 2. Customize ES API Code

Write custom API code by referring to the code generated in step one. The following are code samples:

- Sample 1. Legality Completion Conv2D <a id="sample1"></a> <br>
  Conv2D type operators need to set Format themselves, you can add logic to ensure operator validity
  - [my_Conv2D_c.h](./custom/my_Conv2D_c.h)
  - [my_Conv2D.h](./custom/my_Conv2D.h)
  - [my_Conv2D.cpp](./custom/my_Conv2D.cpp)

- Sample 2. Redundant Attribute Elimination ConcatD <br>
  Due to historical reasons, some attributes defined in IR are redundant <br>
  For example, in ConcatD operator, attribute N represents the number of input x <br>
  The optimization method is to register attribute N as a derivable attribute, so it can be automatically derived from x_num. Register derivation logic through the following statement: `int64_t N = x_num;`
  - [my_ConcatD_c.h](./custom/my_ConcatD_c.h)
  - [my_ConcatD.h](./custom/my_ConcatD.h)
  - [my_ConcatD.cpp](./custom/my_ConcatD.cpp)

- Sample 3. Dynamic Output Parameter Elimination IdentityN <br>
  When generating API, register dynamic output quantity derivation logic for embedded operators, so the output quantity can be automatically determined without explicit parameter passing <br>
  For example, IdentityN can add derivation code, expressing that the dynamic output y quantity can be obtained from "x_num" expression: `int64_t y_num = x_num;`
  - [my_IdentityN_c.h](./custom/my_IdentityN_c.h)
  - [my_IdentityN.h](./custom/my_IdentityN.h)
  - [my_IdentityN.cpp](./custom/my_IdentityN.cpp)

- Sample 4. Pass Custom Attributes
  - [my_Conv2D_c.h](./custom/my_Conv2D_c.h)
  - [my_Conv2D.h](./custom/my_Conv2D.h)
  - [my_Conv2D.cpp](./custom/my_Conv2D.cpp)

### Step 3. Graph Construction

Customize graph construction code logic (refer to [main.cc](./src/main.cc))

### Step 4. Build Project

Key process as follows (refer to [CMakeLists.txt](CMakeLists.txt))

1. Set variables && import cmake functions

2. Define operator prototype library

    ```cmake
    # Note:
    # Current is an out-of-box example, i.e., define INTERFACE to use existing prototype so
    # Normal use will not use existing so, the more common way is to directly use the so target already in the project
    # How to use existing so in project:
    # 1. add_library(opgraph_all SHARED)
    # 2. Set PROPERTIES LIBRARY_OUTPUT_DIRECTORY
    add_library(opgraph_all INTERFACE)
    set_target_properties(opgraph_all PROPERTIES
        INTERFACE_LIBRARY_OUTPUT_DIRECTORY "${ASCEND_INSTALL_PATH}/opp/built-in/op_proto"
    )
    ```

3. Generate ES API package via `add_es_library_and_whl` or `add_es_library`, if you need to exclude some operator ES API generation, add parameter EXCLUDE_OPS

    ```cmake
    add_es_library(
        ES_LINKABLE_AND_ALL_TARGET es_all
        OPP_PROTO_TARGET  opgraph_all
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
        EXCLUDE_OPS       Conv2D,ConcatD
    )
    ```

4. Import build custom ES API package

    ```cmake
    file(GLOB CPP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/custom/*.cpp")
    add_library(self_defined_es_lib SHARED ${CPP_FILES})

    find_library(GRAPH_LIB NAMES graph)

    # Add header file path
    target_include_directories(self_defined_es_lib
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/custom
    )
    ```

5. Custom API package link

    ```cmake
    target_link_libraries(self_defined_es_lib PUBLIC es_all)

    add_dependencies(self_defined_es_lib es_all)
    ```

## Precautions

1. Ensure environment variables are correctly set
2. Ensure sufficient disk space to store generated code files
3. The number of generated code files depends on the number of operators registered in the system
