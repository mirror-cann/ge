# get\_plugin

## 产品支持情况

全量芯片支持。

## 功能说明

获取指定名称的插件模块。

## 函数原型

```python
get_plugin(plugin_name: str) -> Union[ModuleType, None]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| plugin_name | 输入 | 插件名称（例如'math'、'nn'）。 |

## 返回值说明

\(ModuleType | None\)插件模块对象，未找到时返回None。

## 约束说明

无
