# Run

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/decompose\_pass.h\>
- 库文件：libge\_compiler

## 功能说明

使用注册时声明的op type，找到图中匹配的node。

对匹配到的node，依次调用子类定义的MeetRequirements函数，判断是否可被替换；获取到子类定义的replacement，对图中匹配到节点依次进行替换。

注意：Run函数只处理当前图，若需要处理子图，由Pass调用者来负责。

## 函数原型

```c++
Status Run(GraphPtr &graph, CustomPassContext &pass_context) override
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 即将被修改的原始图。 |
| pass_context | 输入 | 自定义Pass上下文。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：替换成功<br>FAILED：替换失败 |

## 约束说明

无
