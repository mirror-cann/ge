# bundle\_save\_model

## 产品支持情况

全量芯片支持。

## 功能说明

将离线模型序列化并保存到指定文件中，该接口适用于权重更新场景。

## 函数原型

```python
bundle_save_model(output_file: str, model: ModelBuffer) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| output_file | 输入 | 保存离线模型的文件名。生成的离线模型文件名会自动以.om后缀结尾，若om文件名中包含操作系统及架构，则该om文件只能在该操作系统及架构的运行环境中使用。 |
| model | 输入 | 离线模型缓冲数据。 |

## 返回值说明

无。

## 约束说明

- 如果output\_file不是字符串，抛出TypeError。
- 如果model不是ModelBuffer类型，抛出TypeError。
- 如果保存失败，抛出RuntimeError。
- 若生成的om模型文件名中含操作系统及架构，但操作系统及其架构与模型运行环境不一致时，需要与OPTION\_HOST\_ENV\_OS、OPTION\_HOST\_ENV\_CPU参数配合使用，设置模型运行环境的操作系统类型及架构。参数具体介绍请参见[aclgrphBuildInitialize支持的配置参数](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta2/API/ascendgraphapi/atlasgeapi_07_0142.html)。
