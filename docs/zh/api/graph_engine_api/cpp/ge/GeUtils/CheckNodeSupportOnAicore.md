# CheckNodeSupportOnAicore

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_utils.h\>
- 库文件：libge\_compiler.so

## 功能说明

对传入的Node做校验，校验其是否支持在AI Core上执行。

## 函数原型

```c++
static Status CheckNodeSupportOnAicore(const GNode &node, bool &is_supported, AscendString &unsupported_reason)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node | 输入 | 待校验的节点。 |
| is_supported | 输出 | 是否支持在AI Core上执行。<br><br>  - true：支持<br>  - false：不支持。 |
| unsupported_reason | 输出 | 不支持的具体原因。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | SUCCESS：校验成功<br>FAILED：校验失败 |

## 约束说明

在调用此接口之前，请确保节点的shape已被推导过。因为校验过程需要结合shape信息进行判断。
