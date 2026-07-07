# build\_initialize

## 产品支持情况

全量芯片支持。

## 功能说明

模型构建初始化，用于申请资源。

## 函数原型

```python
build_initialize(global_options: Optional[dict] = None) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| global_options | 输入 | 全局配置参数。<br>配置参数dict映射表，key为参数类型，value为参数值，均为字符串格式，用于描述离线模型编译初始化信息。<br>dict中支持的配置参数请参见[aclgrphBuildInitialize支持的配置参数](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta2/API/ascendgraphapi/atlasgeapi_07_0142.html)，key为C++常量对应的字符串值。 |

## 返回值说明

无。

## 约束说明

- 如果global\_options不是有效的字符串dict，抛出TypeError。
- 如果初始化失败，抛出RuntimeError。
- 一个进程内只能调用一次build\_initialize接口。
- 调用该接口后，可以在同一进程中连续调用多次build\_model接口。
- **对于build\_model和build\_initialize中重复的options编译配置参数，建议不要重复配置，如果设置重复，则以build\_model传入的为准**。
