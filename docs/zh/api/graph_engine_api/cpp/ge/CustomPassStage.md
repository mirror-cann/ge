# CustomPassStage

自定义Pass执行阶段，头文件：\#include <register/register\_custom\_pass.h\>

```c++
enum class CustomPassStage : uint32_t {
  kBeforeInferShape = 0,
  kAfterInferShape = 1,
  kAfterAssignLogicStream = 2, // only support CustomAllocateStreamPassFunc in this stage
  kAfterBuiltinFusionPass = 3,
  kAfterOriginGraphOptimize=4,
  kCompatibleInherited=5,
  kInvalid
};
```

- kBeforeInferShape：（默认值）自定义Pass在框架入口处InferShape之前执行。
- kAfterInferShape：自定义Pass在InferShape之后执行。

    如果自定义Pass在InferShape之后执行，Pass中需要保证改图之后shape的连续性：

    ```c++
        // 1. 获取输入节点node1的输出描述
        TensorDesc output_desc;
        node1.GetOutputDesc(0, output_desc);
        // 2. 更新当前节点node2的输入描述
        node2.UpdateInputDesc(0, output_desc);
        // 3. 当前节点node2推导InferShape
        operator2.InferShapeAndType();
    ```

    调用InferShape函数时，InferShape之前会将输入的original shape刷入到算子的shape上，InferShape之后会将算子的输出shape刷入到算子输出的original shape上，因此当为一个算子设置InputDesc时，需要设置original shape。

- kAfterAssignLogicStream：自定义Pass在逻辑流分配阶段之后执行。该阶段仅接收逻辑流分配的Pass（注册自定义的逻辑流分配Pass执行函数请参见[CustomAllocateStreamPassFn](./PassRegistrationData/CustomAllocateStreamPassFn.md)），由于该阶段不允许改图，其他场景的改图Pass会校验报错。
- kAfterBuiltinFusionPass：自定义Pass在内置的原图融合Pass之后执行。
- kAfterOriginGraphOptimize：自定义Pass在原图优化阶段后执行。
- kCompatibleInherited：仅适用于内置Pass，用户无需注册该字段。自定义Pass注册该字段不生效。

    > [!NOTE]说明
    >注册到kAfterOriginGraphOptimize和kCompatibleInherited两个阶段的Pass，需对新节点调用[CheckNodeSupportOnAicore](./GeUtils/CheckNodeSupportOnAicore.md)接口，以验证新节点是否支持在AICore上执行。

- kInvalid：不生效。
