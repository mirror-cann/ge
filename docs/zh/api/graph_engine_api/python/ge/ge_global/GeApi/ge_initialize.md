# ge\_initialize

## 功能说明

初始化GE，准备执行。

## 函数原型

```python
ge_initialize(config: dict) -> int
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| config | 输入 | 配置字典。key为参数类型，value为参数值，均为字符串格式，支持的配置项请参见[options参数说明](../../../../cpp/ge/options_params/options_parameters_description.md)中全局级别的参数。 |

## 返回值说明

\(int\) 初始化结果，0表示成功。

## 约束说明

- 如果config不是字典类型，抛出TypeError。
- 如果config为空，抛出TypeError。
- 如果初始化失败，抛出RuntimeError。
- GE不支持多实例运行，一次只能初始化一个。
- 多次调用该接口而没有调用ge\_finalize，运行不可预期。
