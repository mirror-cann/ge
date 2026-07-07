# build\_model

## 产品支持情况

全量芯片支持。

## 功能说明

将输入的Graph编译为适配AI处理器的离线模型，并保存到内存缓冲区。

## 函数原型

```python
build_model(graph: Graph, build_options: Optional[dict] = None) -> ModelBuffer
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| graph | 输入 | 待编译的Graph。 |
| build_options | 输入 | Graph级别配置参数。<br>配置参数dict映射表，key为参数类型，value为参数值，均为字符串格式，用于描述离线模型编译配置信息。<br>dict中支持的配置参数请参见[aclgrphBuildModel支持的配置参数](../../../cpp/ge/aclgrphBuildModel_config_params/aclgrphbuildmodel_config_params.md)，key为C++常量对应的字符串值。 |

## 返回值说明

\(ModelBuffer\)编译生成的离线模型缓存。

## 约束说明

- 如果graph不是Graph类型，抛出TypeError。
- 如果build\_options不是有效的字符串dict，抛出TypeError。
- 如果编译失败，抛出RuntimeError。

- **对于build\_model和build\_initialize中重复的options编译配置参数，建议不要重复配置，如果设置重复，则以build\_model传入的为准**。
- 使用build\_model接口传入build\_options参数时，多张图场景下，如果传入的参数为PRECISION\_MODE或者PRECISION\_MODE\_V2，多张图设置的参数值需要相同。
- 使用build\_model接口编译的离线模型，保存在内存缓冲区中：
  - 如果希望将内存缓冲区中的模型保存为离线模型文件xx.om，则需要调用save\_model接口，序列化保存离线模型到文件中。后续进行推理业务，需要使用**从文件中**加载模型的接口，例如aclmdlLoadFromFile，然后使用aclmdlExecute接口执行推理。
  - 如果离线模型保存在内存缓冲区： 后续进行推理业务时，需要使用**从内存中**加载模型的接口，例如aclmdlLoadFromMem，然后使用aclmdlExecute接口执行推理。

接口详细介绍请参见“[模型管理](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)”章节。
