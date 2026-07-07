# GetCompiledModel

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

获取图编译后的序列化模型。

调用本接口前，必须先调用[CompileGraph](./CompileGraph.md)；用户通过本接口获取内存缓冲区的序列化模型数据ModelBufferData后，后续操作分为两种：

- 用法一：（推荐用法）生成om离线模型
    1. 在线编译：通过GetCompiledModel接口获取编译后的序列化模型。
    2. 调用[aclgrphSaveModel](../aclgrphSaveModel.md)接口将序列化模型保存为om离线模型。

        调用GetCompiledModel接口和调用aclgrphSaveModel接口时，用户应确保所使用的CANN软件包版本**一致**，接口内不做校验。

    3. 调用**从文件中**加载模型接口aclmdlLoadFromFile，然后使用aclmdlExecute接口执行推理。

- 用法二：不生成om离线模型
    1. 在线编译：通过GetCompiledModel接口获取编译后的序列化模型。
    2. 调用**从内存中**加载模型接口aclmdlLoadFromMem，然后使用aclmdlExecute接口执行推理。

推荐使用用法一：因为生成om离线模型后，调用GetCompiledModel接口和调用aclmdlLoadFromFile接口的CANN软件包版本号**可以不一致**；而如果使用用法二，由于内存缓冲区的序列化模型数据ModelBufferData不保证版本兼容性，要求调用GetCompiledModel接口和调用aclmdlLoadFromMem接口的CANN软件包版本号**一致**，否则可能引发未定义行为。

接口详细介绍请参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理”章节。

## 函数原型

```c++
Status GetCompiledModel(uint32_t graph_id, ModelBufferData &model_buffer)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| model_buffer | 输出 | 序列化的模型，保存在内存缓冲区中。详情请参见[ModelBufferData](../ModelBufferData.md)。<br>其中data指向生成的模型数据，length代表该模型的实际大小。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>FAILED：失败 |

## 约束说明

- 变量与转换算子融合会导致图需要重新编译：由于通过GetCompiledModel接口获取序列化模型后无法再次编译，因此若图中包含变量，需将[性能调优](../options_params/overall_constraints.md)  \> ge.exec.variable\_acc参数配置为False，以关闭变量与转换算子的融合功能。
- 关于启用外置权重功能（[内存管理](../options_params/memory_management.md)  \>ge.externalWeight）后的权重文件管理，需注意以下要点：
    - 默认行为：当开启外置权重功能时，系统会默认将权重文件写入一个临时目录（具体路径可参考ge.externalWeight说明）。需要注意的是，在GeSession对象析构时，该临时目录及其下的文件将被自动清理删除。
    - 操作建议：为避免权重文件在GeSession析构后丢失，用户可以参考以下任一方法进行持久化保存：

        - 指定专用目录：建议在初始化时，通过配置ge.externalWeightDir参数，直接指定一个专用的、非临时的路径用于存放权重文件。
        - 提前备份文件：在GeSession对象析构之前，通过应用程序主动将临时路径下的权重文件拷贝至其他指定安全目录。
        - 保存离线模型：在GeSession析构之前，调用aclgrphSaveModel接口，将已编译好的模型（包含权重）序列化保存为独立的om离线模型文件。

        通过以上方式，可以有效管理外置权重文件的生命周期，确保其不会因GeSession的销毁而意外丢失。
