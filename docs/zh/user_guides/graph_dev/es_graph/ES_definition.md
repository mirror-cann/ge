# 什么是ES

本章节介绍一种极简构图的方式，旨在提升构图的易用性。

ES（Eager Style）是一套**函数风格的构图API**，其语法类似Torch Eager模式脚本，因此称为Eager Style； 特点是通过函数表达连边和传递IR（Intermediate Representation）信息，相比[通过算子原型（即IR）构建Graph](../construct_graph/construct_graph_using_op_proto.md)的方式，ES构图的方式可以使用更少的代码量来构图，提升构图易用性的同时兼顾了一定的防呆能力。整体逻辑视图如下：

![](../figures/cann_architecture_5.png)

ES整体分为三个关键组件：

- **ES基础数据结构**：提供[EsGraphBuilder](../../../api/graph_engine_api/cpp/ge/es/EsGraphBuilder/overview.md)、[EsTensorHolder](../../../api/graph_engine_api/cpp/ge/es/EsTensorHolder/overview.md)等ES构图基础数据结构API，详情请参见[ES接口](../../../api/graph_engine_api/cpp/ge/es/es_interface.md)。
- **ES Code Generator**：读取算子原型，生成每个算子的构图API的可执行程序。
- **Generated** **ES 构图API**：由ES Code Generator在构建时生成或者已包含在CANN软件包中，用户可直接使用的构图API。

ES系列构图API有如下特点：

- 基于算子原型自动CodeGen得来。
- 支持多语言：支持C、C++、Python三种语言。
- 从API、ABI、IR语义三个维度支持向前、向后兼容。

## ES构图API与IR的映射关系

在三种语言中，ES构图API与IR的映射逻辑相同，每个IR映射为一个函数，函数名取自IR类型（C接口前面会有Es前缀）。函数参数依次对应算子的输入和属性，返回值对应输出。

例如，算子Foo IR原型定义如下：

```c++
REG_OP(Foo)
    .INPUT(x1)
    .INPUT(x2)
    .OUTPUT(y1)
    .ATTR(a1, Int, 10)
    .ATTR(a2, Int, 20)
    .OP_END_FACTORY_REG(Foo)
```

在上述IR原型定义中，算子类型为Foo，算子输入为x1和x2，输出为y1，属性为a1和a2（默认值分别为10和20）；在ES构图API中，其对应的函数原型为：

- 函数名：Foo（C++ / Python）或EsFoo（C）。
- 参数：共 4 个，依次为x1、x2、a1、a2。
- 返回值：输出y1。

根据上述原则，生成的ES构图API函数原型实例为：

- C语言

    ```c
    EsCTensorHolder* EsFoo(EsCTensorHolder* x1, EsCTensorHolder* x2, int64_t a1,  int64_t a2);
    ```

- C++语言

    ```c++
    namespace ge {
    namespace es {
    EsTensorHolder Foo(const EsTensorLike &x1, const EsTensorLike &x2, int64_t a1 = 10,  int64_t a2 = 20);
    }
    }
    ```

    在C++语言中：

    1. 使用TensorLike类型表达输入，以支持实参可以直接传递数值的情况，比如Foo\(t0, float\(1.0\)\)。
    2. 为表达IR原型中某些属性是可选的，在ES构图API函数原型中可以通过为其设置默认参数来标识。

- Python语言

    ```python
    def Foo(x1: Union[TensorHolder, TensorLike], x2: Union[TensorHolder, TensorLike], *, a1: int = 10, a2: int = 20) -> TensorHolder:
    ```

    在Python语言中，使用位置参数表示输入，关键字参数表示属性。该方式既能清晰区分输入与属性，又便于扩展与兼容。

## 头文件/模块拆分策略

每个IR原型定义在ES中对应一个独立的头文件（C和C++）及一个对应的Python模块（.py文件），例如上述Foo IR原型定义对应生成的ES头文件为：

- C语言：es\_Foo\_c.h
- C++语言：es\_Foo.h
- Python语言：es\_Foo.py

同时为提升多语言使用体验，ES提供聚合接口，单个头文件和聚合文件的命名规则请参考[生成产物说明](generate_es_graph_api_optional.md#生成产物说明)。
