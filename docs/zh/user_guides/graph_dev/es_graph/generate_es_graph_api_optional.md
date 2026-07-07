# 生成ES构图API（可选）

本节介绍如何通过ES Code Generator生成用户可直接使用的构图API。该场景需要用户已经有应用程序，且应用程序中存在CMake文件。

该章节内容为可选，具体使用场景如下：

- **可选场景**，内置算子场景，该场景用户无需再执行该章节：
  - 用户安装的CANN软件包中已经预置了内置算子的ES接口产物，路径为：$\{INSTALL\_DIR\}/include/es。

    其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

  - 用户引用相关头文件链接到对应的so库或者导入相应的Python模块即可以直接调用ES接口。

- **必选场景**，该章节必须执行：
  - 如果安装的CANN软件包中没有预置ES的构图API交付件且需要用ES来构图。
  - 自定义场景：
    - 如果用户自定义的算子（Custom Ops）也想使用ES的API来构建图，此时可以手动执行Codegen，严格按照IR原型生成默认的ES构图API。
    - 如果现有的ES API无法满足需求，可以重新定义一个新的ES API来替代现有的实现。

## 前提条件

- 用户已经安装CANN软件包，并设置如下环境变量：（如下环境变量以root用户为例，且CANN软件包安装在默认路径）

    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

- 用户环境中有如下依赖，且需要满足相应版本要求：
  - CMake版本：\>= 3.17
  - Python3：\>= 3.7（仅add\_es\_library\_and\_whl需要）
  - 安装Python依赖：setuptools、wheel（仅add\_es\_library\_and\_whl需要）

- 用户CMake文件中已经定义原型动态库：

    ```cmake
    add_library(opgraph_math SHARED)
    ```

## 修改CMake文件

1. 引入函数

    ```cmake
    # 添加模块路径，以便CMake可以找到GenerateEsPackage模块
    list(APPEND CMAKE_MODULE_PATH "/usr/local/Ascend/cann/include/ge/cmake")
    # 查找模块
    find_package(GenerateEsPackage REQUIRED)
    ```

    当前版本需要在CMakeLists.txt中手动添加CMAKE\_MODULE\_PATH。

2. 生成ES构图API

    1. 调用add\_es\_library\_and\_whl，生成C/C++动态库、Python wheel包（完整产物：API的载体）：

        ```cmake
        add_es_library_and_whl(
            ES_LINKABLE_AND_ALL_TARGET es_math
            OPP_PROTO_TARGET  opgraph_math # 前提条件中的原型库target
            OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
        )
        ```

    2. 调用add\_es\_library，**只生成**C/C++动态库（纯C/C++项目使用）：

       ```cmake
       add_es_library(
          ES_LINKABLE_AND_ALL_TARGET es_math
          OPP_PROTO_TARGET  opgraph_math
          OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
        )
        ```

    add\_es\_library\_and\_whl和add\_es\_library函数是对gen\_esb二进制工具的封装，包括源文件编译和Python whl包的编译打包等行为；该工具已集成到CANN软件包中，工具详细使用请参见[gen\_esb二进制工具使用](../appendix/gen_esb_tool_usage.md)。函数中详细参数解释请参见下表。

    **表 1**  CMake文件相关参数解释

    |参数|说明|示例|
    |--|--|--|
    |ES_LINKABLE_AND_ALL_TARGET|必填，产物中的Target名称。|es_math、es_nn、es_cv建议使用es_ 前缀（如es_math、es_nn）。使用小写字母和下划线。避免使用特殊字符和C++关键字。|
    |OPP_PROTO_TARGET|必填，算子原型库的CMake Target名称，针对该原型库中的算子生成对应的ES接口。|opgraph_math、opgraph_nn|
    |OUTPUT_PATH|可选，产物输出的根目录。|${CMAKE_BINARY_DIR}/output|
    |EXCLUDE_OPS|可选，需要排除生成的算子|Add、Conv2D|

3. 使用生成的产物

    ```cmake
    # 创建一个名为my_app的可执行文件，源文件为main.cpp
    add_executable(my_app main.cpp)
    # 将my_app可执行文件与es_math目标链接，PRIVATE表示依赖关系仅对my_app本身可见
    target_link_libraries(my_app PRIVATE es_math)
    ```

    > [!NOTE]说明
    >
    >因为ES构图API是基于算子IR原型生成，部分算子IR无法支撑所有信息表达（比如Conv类算子）或者IR表达有冗余信息（比如IdentityN算子），所以推荐用户自行封装ES API，该场景下用户可以自定义ES API，详细介绍请参见[自定义ES API](https://gitcode.com/cann/ge/blob/master/examples/custom_es_api)。

如下是一个完整的CMake文件示例，其中的CANN软件包安装路径/usr/local/Ascend/请根据实际情况进行替换，更多CMake示例请参见[CMake常见示例](../appendix/cmake_examples.md)。

```cmake
# ===== 基础设置 =====
cmake_minimum_required(VERSION 3.17)
project(my_es_project LANGUAGES CXX)

# ===== 引入函数（推荐：使用 find_package） =====
list(APPEND CMAKE_MODULE_PATH "/usr/local/Ascend/cann/include/ge/cmake")
find_package(GenerateEsPackage REQUIRED)

# ===== 第一部分: 前置条件：定义原型库 =====
add_library(opgraph_math SHARED
)

# ===== 第二部分: 生成 ES API包 =====
add_es_library_and_whl(
    ES_LINKABLE_AND_ALL_TARGET es_math
    OPP_PROTO_TARGET  opgraph_math
    OUTPUT_PATH       ${CMAKE_BINARY_DIR}/output
)

# ===== 第三部分: 在应用中使用 =====
add_executable(my_app
    src/main.cpp
    src/inference.cpp
)

# 链接ES_LINKABLE_AND_ALL_TARGET
target_link_libraries(my_app PRIVATE
    es_math  # 自动获得依赖、头文件和库
)
```

构建命令为：

```bash
make my_app
# 输出：
# [Smart Build] Step 1: Building internal targets...
# [Smart Build] Found xx generated source file(s)
# [Smart Build] Step 2: Reconfiguring to include generated sources...
# [Smart Build] Step 3: Rebuilding with all generated sources...
# [Smart Build] Successfully built ES package 'es_math' with xx source file(s)
# Built target my_app
```

## 生成产物说明

- add\_es\_library\_and\_whl生成产物为：

    ```text
    OUTPUT_PATH/
    ├── include/
    │   └── es_math/               # 头文件目录
    │       ├── es_math_ops.h      # C++接口聚合头文件，聚合头文件表示包含es_math下所有算子的构图API
    │       ├── es_math_ops_c.h    # C接口聚合头文件
    │       └── es_Add.h            # 单个算子头文件
    │       └── es_xxx.h            # 其他算子原型对应的ES构图头文件
    ├── lib64/
    │   └── libes_math.so          # 动态库
    └── whl/
        └── es_math-1.0.0-py3-none-any.whl  # Python包，用户使用Python语言构图时，需要手动安装该软件包
    ```

- add\_es\_library生成产物为：

    ```text
    OUTPUT_PATH/
    ├── include/
    │   └── es_math/               # 头文件目录
    │       ├── es_math_ops.h      # C++接口聚合头文件
    │       ├── es_math_ops_c.h    # C接口聚合头文件
    │       └── es_Add.h           # 单个算子头文件
    │       └── es_xxx.h           # 其他算子原型对应的ES构图头文件
    └── lib64/
    │   └── libes_math.so          # 动态库
    └── whl/                        # C/C++场景下不会生成whl包，该目录为空
    ```

产物命名规则如下：

**表 2**  产物命名规则

|产物类型|命名规则|示例 (以ES_LINKABLE_AND_ALL_TARGET=es_math为例)|
|--|--|--|
|动态库|lib<ES_LINKABLE_AND_ALL_TARGET>.so|libes_math.so|
|动态库：对外库|<ES_LINKABLE_AND_ALL_TARGET>|es_math建议使用es_ 前缀（如es_math、es_nn）。使用小写字母和下划线。避免使用特殊字符和C++关键字。|
|Python包|<ES_LINKABLE_AND_ALL_TARGET>-1.0.0-py3-none-any.whl|es_math-1.0.0-py3-none-any.whl|
|聚合头文件|es_\<name>_ops.h|es_math_ops.h|
