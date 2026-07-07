# ResolveBuilder

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_like.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

无参数时返回nullptr或者从Tensor对象/向量中获取owner builder。

## 函数原型

- 无参数时返回nullptr

    ```c++
    EsCGraphBuilder *ResolveBuilder()
    ```

- ensor对象/向量中获取owner builder

    ```c++
    EsCGraphBuilder *ResolveBuilder(const T& first, const Ts&... rest)
    ```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| first | 输入 | Tensor对象/向量。 |
| rest | 输入 | 剩余的Tensor对象/向量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCGraphBuilder * | - 函数原型1：返回nullptr<br>  - 函数原型2：返回C层builder指针。 |

## 约束说明

无
