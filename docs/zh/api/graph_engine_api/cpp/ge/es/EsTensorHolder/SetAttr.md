# SetAttr

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置TensorHolder的私有属性。

## 函数原型

```c++
Status SetAttr(const char *attr_name, int64_t value)
Status SetAttr(const char *attr_name, const char *value)
Status SetAttr(const char *attr_name, bool value)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr_name | 输出 | 属性名称。 |
| value | 输入 | 属性值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GRAPH_SUCCESS(0)：成功<br>其他值：失败 |

## 约束说明

无

## 调用示例

```c++
  auto builder = std::make_unique<ge::EsGraphBuilder>("graph");
  auto tensor0 = builder->CreateInput(0);
  (void) tensor0.SetAttr("attr_int64", int64_t(123)); // 设置输入节点输出tensor的私有属性，属性名"attr_int64"，属性值123
```
