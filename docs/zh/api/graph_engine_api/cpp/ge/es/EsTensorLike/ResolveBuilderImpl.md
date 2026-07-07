# ResolveBuilderImpl

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_like.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取owner builder。

## 函数原型

- 从EsTensorLike对象中解析所属的EsCGraphBuilder

    ```c++
    EsCGraphBuilder *ResolveBuilderImpl(const EsTensorLike &tensor_like)
    ```

- 从EsTensorHolder向量中解析所属的EsCGraphBuilder

    ```c++
    EsCGraphBuilder *ResolveBuilderImpl(const std::vector<EsTensorHolder> &tensors)
    ```

- 从EsCTensorHolder\* 中解析所属的EsCGraphBuilder

    ```c++
    EsCGraphBuilder *ResolveBuilderImpl(EsCTensorHolder *esb_tensor)
    ```

- 从EsCTensorHolder\* 向量中解析所属的EsCGraphBuilder

    ```c++
    EsCGraphBuilder *ResolveBuilderImpl(const std::vector<EsCTensorHolder *> &esb_tensors)
    ```

- 从EsGraphBuilder指针中获取底层的EsCGraphBuilder

    ```c++
    EsCGraphBuilder *ResolveBuilderImpl(const EsGraphBuilder *graph_builder)
    ```

- 直接返回已是EsCGraphBuilder\*的参数

    ```c++
    EsCGraphBuilder *ResolveBuilderImpl(EsCGraphBuilder *graph_builder)
    ```

- 处理nullptr\_t参数的实现

    ```c++
    EsCGraphBuilder *ResolveBuilderImpl(const std::nullptr_t)
    ```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor_like | 输入 | EsTensorLike对象。 |
| tensors | 输入 | EsTensorHolder向量。 |
| esb_tensor | 输入 | EsCTensorHolder指针向量。 |
| graph_builder | 输入 | 图构建指针。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCGraphBuilder * | C层builder指针。 |

## 约束说明

无
