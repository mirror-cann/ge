# CreateVariable

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建变量节点。

## 函数原型

```c++
EsTensorHolder CreateVariable(int32_t index, const char *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 变量节点索引。 |
| name | 输入 | 变量节点名称，var的名称相同代表共享，因此需要显式指定。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsTensorHolder | 返回创建的变量张量持有者，失败时返回无效的EsTensorHolder。 |

## 约束说明

无

## 调用示例

```c++
EsGraphBuilder builder("test_graph");
auto v1 = builder.CreateVariable(0, "var0");
```
