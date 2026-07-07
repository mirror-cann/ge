# get\_registered\_pass\_dicts

## 产品支持情况

全量芯片支持。

## 功能说明

获取所有已注册Pass的字典表示列表。

## 函数原型

```python
get_registered_pass_dicts() -> List[dict]
```

## 参数说明

无

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| List[dict] | 返回已注册Pass的字典列表，每个字典包含descriptor_key、pass_name、module_name、class_name、stage、kind、op_types等字段。 |

## 约束说明

无
