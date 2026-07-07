# CMake常见示例

- 基本用法

    ```cmake
    # 1. 定义原型动态库（前置条件）
    add_library(opgraph_math SHARED
    )
    # 2. 生成ES API包（库+wheel包）
    add_es_library_and_whl(
        ES_LINKABLE_AND_ALL_TARGET es_math
        OPP_PROTO_TARGET  opgraph_math
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
    )

    # 3. 在应用中使用
    add_executable(my_app main.cpp)
    target_link_libraries(my_app PRIVATE es_math)
    ```

    如果工程中还有在编译阶段就需要ES头文件的OBJECT/STATIC中间目标，可额外添加add\_dependencies\(<中间目标\> build\_es\_math\)（build\_es\_\*\*\* 名称会随包名变化），以确保在编译该目标前已完成代码生成。最终的可执行或共享库只需target\_link\_libraries\(... es\_math\) 即可自动触发相关依赖，示例如下：

    ```cmake
    add_library(my_obj OBJECT foo.cc bar.cc)
    # 确保在编译my_obj之前，先执行build_es_math这个目标，通常用于生成依赖的头文件等前置操作
    # 比如es_math头文件是通过build_es_math生成的
    add_dependencies(my_obj build_es_math)
    # 将es_math的头文件搜索路径传递给my_obj
    target_link_libraries(my_obj PRIVATE es_math)

    # 创建可执行文件my_app，使用my_obj中生成的目标文件（$<TARGET_OBJECTS:my_obj>）
    add_executable(my_app $<TARGET_OBJECTS:my_obj>)
    将es_math的依赖链接到my_app（可选）
    target_link_libraries(my_app PRIVATE
            es_math
    )
    ```

- 只生成C/C++ 库

    ```cmake
    # 只生成 C/C++ 库（跳过Python wheel包）
    add_es_library(
        ES_LINKABLE_AND_ALL_TARGET es_math
        OPP_PROTO_TARGET  opgraph_math
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
    )
    ```

- 排除部分算子生成

    ```cmake
    # 生成math TARGET
    # 生成产物不会包含es_Add_c.h; es_Add.h, es_Add.cpp, es_Add.py
    add_es_library(
        ES_LINKABLE_AND_ALL_TARGET es_math
        OPP_PROTO_TARGET  opgraph_math
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
        EXCLUDE_OPS       Add
    )
    ```

- 多个TARGET

    ```cmake
    # 生成math TARGET
    add_es_library_and_whl(
        ES_LINKABLE_AND_ALL_TARGET es_math
        OPP_PROTO_TARGET  opgraph_math
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
    )

    # 生成nn TARGET
    add_es_library_and_whl(
        ES_LINKABLE_AND_ALL_TARGET es_nn
        OPP_PROTO_TARGET  opgraph_nn
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
    )

    # 使用多个TARGET
    add_executable(my_inference_app main.cpp)
    target_link_libraries(my_inference_app PRIVATE
        es_math
        es_nn
    )
    ```

- 多个TARGET共享输出目录：所有TARGET可以使用同一个OUTPUT\_PATH，头文件会按包名组织在子目录中，示例如下：

    ```cmake
    add_es_library_and_whl(
        ES_LINKABLE_AND_ALL_TARGET es_math
        OPP_PROTO_TARGET  opgraph_math
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output  # 共享路径
    )

    add_es_library_and_whl(
        ES_LINKABLE_AND_ALL_TARGET es_nn
        OPP_PROTO_TARGET  opgraph_nn
        OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output  # 共享路径
    )

    # 输出结构：
    # output/
    # ├── include/
    # │   ├── es_math/
    # │   └── es_nn/
    # ├── lib64/
    # │   ├── libes_math.so
    # │   └── libes_nn.so
    # └── whl/
    #     ├── es_math-1.0.0-py3-none-any.whl
    #     └── es_nn-1.0.0-py3-none-any.whl
    ```
