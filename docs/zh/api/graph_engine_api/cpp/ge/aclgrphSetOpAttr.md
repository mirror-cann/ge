# aclgrphSetOpAttr

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

用于支持设置node属性方式的改图。

## 函数原型

```c++
graphStatus aclgrphSetOpAttr(Graph &graph, aclgrphAttrType attr_type, const char_t *cfg_path)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入/输出 | 作为输入时，代表要通过属性方式修改的图；<br>作为输出时表示已经修改后的图。 |
| attr_type | 输入 | attr_type的枚举类型定义：<br>enum aclgrphAttrType { ATTR_TYPE_KEEP_DTYPE = 0, ATTR_TYPE_WEIGHT_COMPRESS };<br>ATTR_TYPE_KEEP_DTYPE：保持模型编译时个别算子的计算精度不变。<br>ATTR_TYPE_WEIGHT_COMPRESS ：模型编译时对个别算子进行weight压缩。 |
| cfg_path | 输入 | 配置文件路径。配置文件举例如下所示。 |

配置文件举例：

计算精度不变的文件格式为：

```text
Opname1
Opname2
…
```

weight压缩文件格式为：

```text
Opname1;Opname2
```

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
