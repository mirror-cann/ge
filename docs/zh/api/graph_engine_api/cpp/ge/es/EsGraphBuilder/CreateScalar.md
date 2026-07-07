# CreateScalar

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建标量常量。

## 函数原型

- 创建int64类型的标量常量

```c++
    EsTensorHolder CreateScalar(int64_t value)
    ```

-   创建int32类型的标量常量

    ```
    EsTensorHolder CreateScalar(int32_t value)
    ```

-   创建uint64类型的标量常量

    ```
    EsTensorHolder CreateScalar(uint64_t value)
    ```

-   创建uint32类型的标量常量

    ```
    EsTensorHolder CreateScalar(uint32_t value)
    ```

-   创建float类型的标量常量

    ```
    EsTensorHolder CreateScalar(float value)
    ```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| value | 输入 | 标量值。 |

## 返回值说明


| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsTensorHolder | 返回创建的标量张量持有者，失败时返回无效的EsTensorHolder。 |

## 约束说明

无

## 调用示例

```c++
EsGraphBuilder builder("test_graph");
auto s1 = builder.CreateScalar(int64_t(42));
```
