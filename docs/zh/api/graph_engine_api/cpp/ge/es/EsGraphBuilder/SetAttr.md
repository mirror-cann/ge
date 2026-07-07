# SetAttr

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置图的类型属性。

## 函数原型

- 设置图的int64类型属性

```c++
    Status SetAttr(const char *attr_name, int64_t value)
    ```

-   设置图的字符串类型属性

    ```
    Status SetAttr(const char *attr_name, const char *value)
    ```

-   设置图的布尔类型属性

    ```
    Status SetAttr(const char *attr_name, bool value)
    ```

## 参数说明


| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr_name | 输入 | 属性名称。 |
| value | 输入 | 属性值。 |

## 返回值说明


| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：操作成功<br>其他：操作失败 |

## 约束说明

无

## 调用示例

```c++
EsGraphBuilder builder("test_graph");
builder.SetAttr("int64_attr", static_cast<int64_t>(10))
```
