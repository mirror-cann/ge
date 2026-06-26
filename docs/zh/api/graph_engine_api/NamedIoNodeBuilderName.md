# Name<a name="ZH-CN_TOPIC_0000002520225199"></a>

## 产品支持情况<a name="section789110355113"></a>

| 产品 | 是否支持 |
| :--- | :---: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 头文件/库文件<a name="section710017525391"></a>

-   头文件：#include <graph/named_io_node_builder.h>
-   库文件：libgraph.so、libgraph_static.a

## 功能说明<a name="section44282629"></a>

设置节点名称（选填）。若不调用此方法，节点名称为空字符串，Build仍可成功。

## 函数原型<a name="section1831611148521"></a>

```
NamedIoNodeBuilder &Name(const char_t *name)
```

## 参数说明<a name="section62999332"></a>

| 参数名 | 输入/输出 | 描述 |
| :--- | :---: | :--- |
| name | 输入 | 节点名称，用于标识图中该节点。 |

## 返回值说明<a name="section30123065"></a>

返回构建器引用，支持链式调用。

## 约束说明<a name="section24049041"></a>

-   该方法为选填项，未调用Name时节点名称为空，不影响Build。
-   若传入nullptr，Name不会被设置，节点名称为空字符串，Build仍可成功。
