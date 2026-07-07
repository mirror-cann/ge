# get\_registered\_pass\_by\_descriptor\_key

## 产品支持情况

全量芯片支持。

## 功能说明

根据描述符键获取已注册的Pass描述符。

## 函数原型

```python
get_registered_pass_by_descriptor_key(descriptor_key: str) -> Optional[PassDescriptor]
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| descriptor_key | 输入 | Pass描述符键，字符串类型，格式为{module_name}:{class_name}:{pass_name}。 |

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| Optional[PassDescriptor] | 返回匹配的PassDescriptor对象；若未找到则返回None。 |

## 约束说明

无
