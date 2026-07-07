# 内存管理

## EXEC\_DISABLE\_REUSED\_MEMORY

内存复用开关。此参数实际对应的options参数为`ge.exec.disableReuseMemory`。

内存复用是指按照生命周期和内存大小，把不冲突的内存重复使用，来降低网络内存占用。

**参数取值：**

- 0：（默认值）开启内存复用。
- 1：关闭内存复用。如果网络模型较大，关闭内存复用开关，会造成后续推理时Device侧内存不复用，从而导致内存不足。

**配置示例：**

```c++
{ge::ir_option::EXEC_DISABLE_REUSED_MEMORY, "0"}
```

**产品支持情况：**

全量芯片支持。

## EXTERNAL\_WEIGHT

生成om模型文件时，是否将原始网络中的Const/Constant节点的权重外置，同时将节点类型转换为FileConstant类型。此参数实际对应的options参数为`ge.externalWeight`。

离线场景，如果模型权重较大且环境对om离线模型大小有限制，建议开启外置权重将权重单独保存，来减小om大小。

**参数取值：**

- 0：（默认值）权重不外置，直接保存在om离线模型文件中。
- 1：权重外置但不归一，将网络中所有Const/Constant节点的权重文件落盘，并将节点类型转换为FileConstant类型；权重文件保存在与om文件同层级的weight目录下，不同节点权重保存到不同的文件中，以weight\_<hash值\>命名。
- 2：权重外置且归一，将网络中所有Const/Constant节点的权重文件落盘，并将节点类型转换为FileConstant类型；权重文件保存在与om文件同层级的weight目录下，所有权重保存到同一个文件中，以“原始图根图名称\_weight\_combined”命名。

**配置示例：**

```c++
{ge::ir_option::EXTERNAL_WEIGHT, "1"}
```

**使用约束：**

- 权重外置场景，在开发推理应用、加载模型时：

  - 使用[aclgrphSaveModel](../aclgrphSaveModel.md)接口保存om模型：
    - 若使用aclmdlLoadFromFile接口加载模型，需将权重文件保存在与om文件同层级的weight目录下。
    - 若使用aclmdlSetConfigOpt和aclmdlLoadWithConfig接口加载模型，对权重外置目录没有要求，后续加载模型时，通过aclmdlLoadWithConfig接口指定权重外置目录。

  - 权重更新场景，使用[aclgrphBundleSaveModel](../aclgrphBundleSaveModel.md)接口保存om模型：

    只能使用[aclmdlBundleLoadFromFile](../../../c/acl/aclmdlBundleLoadFromFile.md)接口加载模型，并且需将权重文件保存在与om文件同层级的weight目录下。

**产品支持情况：**

全量芯片支持。
