# Session构造函数

## 功能说明

初始化一个Session。

## 函数原型

```python
Session(options: Optional[dict] = None)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| options | 输入 | 配置选项字典，可选参数。支持的配置项请参见[options参数说明](../../../../cpp/ge/options_params/options_parameters_description.md)中session级别的参数。 |

## 返回值说明

无

## 约束说明

- 如果options不是字典类型，抛出TypeError。
- 如果会话创建失败，抛出RuntimeError。
