# 基于Pattern匹配实现Pass

为提高自定义融合Pass的开发效率，本章提供了一组基于图结构的匹配与替换接口，用于实现Pass的构建。整个融合行为的逻辑可分为以下三个步骤：

1. 匹配：通过Pattern来定义一种图结构，进行子图结构的匹配查找。
2. 决策：根据匹配结果，结合更具体的条件，判断该匹配是否满足融合要求。
3. 替换：在确认可融合后，执行图结构的替换操作，完成优化。

**图 1**  逻辑架构图<a id="fig1"></a>
![图示](../figures/logical_architecture.png "逻辑架构图")

逻辑架构如上图所示，相关核心概念解释如下：

- Pattern（模式）：在图匹配过程中，Pattern用于描述子图结构特征的模板或规则，通过图匹配算法在Graph中查找符合特定规则的子图。
- PatternMatcher（匹配器）：执行匹配算法的核心对象，它接收一个Pattern，并按照Pattern中定义的子图结构去Graph中查找符合定义的子图。
- GraphRewriter（重写器）：执行改图的核心对象，接收匹配到的子图边界及目标Replacement图（替换后），将原子图节点替换为Replacement中的节点结构，完成图的重构。

开发者通过继承GE提供的基类并重写其方法来实现自定义融合Pass， 接着调用注册宏将Pass注册到指定阶段，根据应用场景的不同，GE提供两类基类供继承：

- 通用子图融合（1:1或复杂拓扑替换）场景：适用于需匹配完整子图结构并整体替换为另一子图的场景

    继承[PatternFusionPass](../../../api/graph_engine_api/cpp/ge/fusion/PatternFusionPass/PatternFusionPass.md)类实现自定义融合Pass类，并通过[REG\_FUSION\_PASS](../../../api/graph_engine_api/cpp/ge/fusion/REG_FUSION_PASS.md)注册宏将Pass注册到指定阶段。

- 单节点替换（单节点替换为N个节点）场景，继承[DecomposePass](../../../api/graph_engine_api/cpp/ge/DecomposePass/DecomposePass.md)类实现自定义融合Pass类，并通过[REG\_DECOMPOSE\_PASS](../../../api/graph_engine_api/cpp/ge/REG_DECOMPOSE_PASS.md)注册宏将Pass注册到指定阶段。

两种场景在使用方法上类似，区别在于匹配对象的粒度（子图/单节点）与替换方式（复杂拓扑替换/多节点展开），下面将详细展开介绍。

## 场景介绍

### 通用子图融合（1:1或复杂拓扑替换）场景融合Pass开发

本节首先对该场景下涉及的核心数据结构PatternFusionPass进行介绍，其次介绍开发过程中需要重写的3个函数：Patterns、MeetRequirements与Replacement，最后介绍如何将Pass注册到指定阶段。

**PatternFusionPass**类声明如下：

```c++
class PatternFusionPass : public FusionBasePass {
  public:
  Status Run(GraphPtr &graph, CustomPassContext &pass_context) override;
     protected:
      virtual std::vector<PatternUniqPtr> Patterns() = 0;
      virtual bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result);
      virtual GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) = 0;
    };
```

Run函数调用Patterns获取模板的拓扑Pattern，将Pattern在目标Graph中逐一匹配，调用MeetRequirements对匹配到的Pattern作出是否需要被替换的判断， 最后通过Replacement获取目标结构，将满足替换条件的Pattern进行替换。

开发者通过继承PatternFusionPass类并重写Patterns、MeetRequirements与Replacement函数实现自定义融合Pass的开发。函数介绍如下：

|函数|说明|是否必须重写|
|--|--|--|
|Patterns|定义在目标图中匹配的模板拓扑；返回一个或多个图结构指针。|是|
|MeetRequirements|对Patterns匹配到的图结构按条件进行过滤；输入匹配结果，返回布尔值。|否，默认直接返回true|
|Replacement|定义替换结构；输入匹配结果，返回图指针。|是|

#### Patterns

Patterns用于定义目标图中匹配的一个或多个模板拓扑，使用[EsGraphBuilder](../../../api/graph_engine_api/cpp/ge/es/EsGraphBuilder/EsGraphBuilder.md)构建一张DAG（Directed Acyclic Graph，有向无环图）图来表达Pattern，如下所示：

```c++
std::vector<PatternUniqPtr> Patterns() override {
  std::vector<PatternUniqPtr> patterns;
  // 使用EsGraphBuilder构建pattern
  auto graph_builder = es::EsGraphBuilder("pattern");
  // 此处定义pattern
  // ...
  // 初始化Pattern对象，xxx请替换为实际输出节点
  auto graph = graph_builder.BuildAndReset({xxx});
  auto pattern = std::make_unique<Pattern>(std::move(*graph));
  patterns.emplace_back(std::move(pattern));
  // 可以继续向patterns中添加多个pattern
  // ...
  return patterns;
}

```

其中，EsGraphBuilder为图构建器类，用于构建计算图。 推荐开发者使用[ES接口](../../../api/graph_engine_api/cpp/ge/es/es_interface.md)进行Pattern的定义，其提供了定义输入、常量与算子等接口，以下是使用ES API定义一个ReLu单算子pattern的示例：

```c++
std::vector<PatternUniqPtr> patterns;
// 创建一个EsGraphBuilder实例，用于构建计算图，图的名称为pattern
auto graph_builder = es::EsGraphBuilder("pattern");
auto data = graph_builder.CreateInput(0);
auto relu = es::Relu(data);
// 构建并重置图，将{relu}作为输出节点
auto graph = graph_builder.BuildAndReset({relu});
// 将graph移动到Pattern构造函数中创建pattern对象
auto pattern = std::make_unique<Pattern>(std::move(*graph));
patterns.emplace_back(std::move(pattern));

```

> [!NOTE]说明
>
>用于匹配的Pattern需要满足自包含（除了边界的输出算子，边界内所有算子的数据输出消费者都要在边界内），非自包含的Pattern不会被匹配。

除了上文中提到的使用EsGraphBuilder构建Pattern，GE还提供了两种接口实现对Pattern更细粒度的定义：

- [CaptureTensor](../../../api/graph_engine_api/cpp/ge/fusion/Pattern/CaptureTensor.md)

  定义过程中可以捕获Pattern中的一个Tensor，从而在[MatchResult](../../../api/graph_engine_api/cpp/ge/fusion/MatchResult/MatchResult.md)中可以按序获取。方法声明如下，入参node\_output为NodeIo类型，由节点与索引组成，表示为某个节点的某个输出。

  ```c++
  // CaptureTensor声明
  Pattern &CaptureTensor(const NodeIo &node_output);

  // NodeIo结构体
  struct NodeIo {
   GNode node;
    int64_t index;
  };

  ```

  调用CaptureTensor捕获data示例如下：

  ```c++
  std::vector<PatternUniqPtr> patterns;
  // 创建一个EsGraphBuilder实例，用于构建计算图，图的名称为pattern
  auto graph_builder = es::EsGraphBuilder("pattern");
  auto data = graph_builder.CreateInput(0);
  auto relu = es::Relu(data);
  // 构建计算图，构建的图仅包含data -> ReLU(relu)的结构
  auto graph = graph_builder.BuildAndReset({relu});
  // 创建一个Pattern实例，用构建好的图初始化
  auto pattern = std::make_unique<Pattern>(std::move(*graph));
  // 调用CaptureTensor捕获data
  pattern->CaptureTensor({*relu.GetProducer(), 0})
  patterns.emplace_back(std::move(pattern));
  ```

- [PatternMatcherConfig](../../../api/graph_engine_api/cpp/ge/fusion/PatternMatcherConfig/PatternMatcherConfig.md)

     构造自定义Pass可以传入PatternMatcherConfig，以开启Const值匹配功能以及IR属性及其值的匹配能力。基类PatternFusionPass构造函数如下：

    ```c++
    explicit PatternFusionPass(std::unique_ptr<PatternMatcherConfig> match_config);
    ```

    使用[PatternMatcherConfigBuilder](../../../api/graph_engine_api/cpp/ge/fusion/PatternMatcherConfigBuilder/PatternMatcherConfigBuilder.md)来构造PatternMatcherConfig，类PatternMatcherConfigBuilder提供两个函数作为匹配能力的开关：

  - EnableConstValueMatch：开启Const值匹配，在匹配过程中将对Pattern中定义的Const/Constant进行值的匹配，值相等才认为匹配成功。
  - EnableIrAttrMatch：开启IR属性及其值匹配，Pass将在Pattern匹配过程中对Pattern中节点上携带的IR属性的数量和值进行匹配。

  以下为名为CustomFusionPass的自定义Pass类打开Const值匹配的构造函数示例：

  ```c++
  explicit CustomFusionPass() : PatternFusionPass(PatternMatcherConfigBuilder().EnableConstValueMatch().Build()) {}
  ```

#### MeetRequirements

对于Patterns获取到的匹配结果，在MeetRequirements中进行筛选。 从上文Run函数的实现中可以看到，每个[MatchResult](../../../api/graph_engine_api/cpp/ge/fusion/MatchResult/MatchResult.md)类型的匹配结果作为MeetRequirements的入参，通过MatchResult开发者可以获取匹配结果的信息进行筛选，最后返回的布尔值作为是否替换该匹配结果的依据，如下所示：

```c++
bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
  // 可以使用传入的match_result对匹配结果进行筛选
  // 满足条件返回true
  if (IsSatisfy(match_result)) {
    return true;
  }
  // 不满足条件返回false
  return false;
}
```

MatchResult是匹配结果类，包含匹配结果的节点、连边等信息。开发者可以使用MatchResult成员函数获取匹配结果的相关信息以进行筛选，以下是使用[GetCapturedTensor](../../../api/graph_engine_api/cpp/ge/fusion/MatchResult/GetCapturedTensor.md)成员函数校验ReLu输出是否为动态shape的示例：

```c++
NodeIo relu_output;
// 尝试从match_result中获取第一个捕获的输出张量，存储到relu_output
if(match_result->GetCapturedTensor(0,relu_output) != GRAPH_SUCCESS){
  return false;
}
TensorDesc relu_out_tensor_desc;
// 从relu_output中获取输出张量描述信息
relu_output.node.GetOutputDesc(relu_output.index, relu_out_tensor_desc);
if (relu_out_tensor_desc.GetShape().GetShapeSize() != -1){
  return false;
}
return true;
```

#### Replacement

Replacement中定义目标结构，替换与Patterns中匹配且MeetRequirements为true的部分。与Patterns一样，使用EsGraphBuilder定义结构，此处不再赘述：

```c++
GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
  auto replacement_graph_builder = es::EsGraphBuilder("replacement");
  // 此处定义替换结构
  // ...
  return replacement_graph_builder.BuildAndReset({r_a});
}
```

> [!NOTE]说明
>
> 如果Pass注册阶段在InferShape后，需要在Replacement中自行调用GeUtils::[InferShape](../../../api/graph_engine_api/cpp/ge/GeUtils/InferShape.md)，此外如果要使用GeUtils::[CheckNodeSupportOnAicore](../../../api/graph_engine_api/cpp/ge/GeUtils/CheckNodeSupportOnAicore.md)判断目标结构是否支持，该函数的调用需要在InferShape之后。

**注册自定义融合Pass**：

完成对融合pass的定义后，需要使用注册宏[REG\_FUSION\_PASS](../../../api/graph_engine_api/cpp/ge/fusion/REG_FUSION_PASS.md)将其注册到对应阶段，如下是将名为CustomFusionPass的自定义Pass注册到kBeforeInferShape阶段的示例：

```c++
REG_FUSION_PASS(CustomFusionPass).Stage(CustomPassStage::kBeforeInferShape);
```

各阶段详细说明请参见[Stage](../../../api/graph_engine_api/cpp/ge/fusion/FusionPassRegistrationData/Stage.md)。

### 单节点替换（单节点替换为N个节点）场景融合Pass开发

该场景下的Pass继承的基类为DecomposePass。由于被替换结构是单个节点，此处Pattern不再需要通过Patterns定义，而是在构造函数中直接传入算子类型，如下所示：

```c++
class CustomOne2NPass : public DecomposePass {
 public:
  CustomOne2NPass(const std::vector<AscendString> &op_types) : DecomposePass(op_types) {}
};
```

与一般场景类似，继承自DecomposePass的Pass也需要重写MeetRequirements与Replacement，但两方法的入参类型不再是MatchResult而是[GNode](../../../api/graph_engine_api/cpp/ge/GNode/GNode.md)，即通过构造时传入的op\_types在图中匹配到的节点。

```c++
bool MeetRequirements(const GNode &matched_node) override {
    ...
}
GraphUniqPtr Replacement(const GNode &matched_node) override {
    ...
}
```

**注册自定义融合Pass**：

如下是使用注册宏[REG\_DECOMPOSE\_PASS](../../../api/graph_engine_api/cpp/ge/REG_DECOMPOSE_PASS.md)将Conv2D作为op\_types初始化CustomOne2NPass，并将其注册在kAfterInferShape的示例：

```c++
REG_DECOMPOSE_PASS(CustomOne2NPass, {"Conv2D"}).Stage(CustomPassStage::kAfterInferShape);
```

## 开发示例

此处以MatMul+Add结构融合为GEMM自定义Pass为例（对应上述一般场景下的融合Pass开发），详细介绍如何通过自定义融合Pass修改Graph，详细可以参见[样例源码](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/pattern_base_pass/1_fuse_matmul_add_pass/cpp)。样例仓还提供了更多样例，用户可以单击[融合Pass样例](https://gitcode.com/cann/ge/tree/master/examples/fusion_pass/pattern_base_pass)进行查看。

修改前后的图结构如下，本例识别图中左边的MatMul+Add结构并通过图修改接口替换为右边的单个GEMM节点：

```text
// |o>-----------------------------------
// |o>      a  b
// |o>      \ /                a    b    c
// |o>     MatMul     c   ==>   \   |   /
// |o>        \      /            GEMM
// |o>           Add
// |o>-----------------------------------
```

1. 包含的头文件。

    ```c++
    #include <iostream>
    // 自定义融合Pass接口头文件
    #include "ge/fusion/pass/pattern_fusion_pass.h"
    // ES接口头文件
    #include "es_all_ops.h"
    ```

2. 使用自定义Pass修改Graph。

    ```c++
    class FuseMatMulAndAddPass : public PatternFusionPass {
     protected:
      // 重写Patterns
      std::vector<PatternUniqPtr> Patterns() override {
        std::cout << "Define pattern for FuseMatMulAndAddPass" << std::endl;
        std::vector<PatternUniqPtr> patterns;
        // 创建一个EsGraphBuilder实例，用于构建计算图，图的名称为pattern0
        auto graph_builder0 = es::EsGraphBuilder("pattern0");
        auto [a0, b0, c0] = graph_builder0.CreateInputs<3>();
        auto matmul0 = es::MatMul(a0, b0);
        auto add0 = es::Add(matmul0, c0);
        // 构建并重置图
        auto graph0 = graph_builder0.BuildAndReset({add0});
        auto pattern0 = std::make_unique<Pattern>(std::move(*graph0));
        patterns.emplace_back(std::move(pattern0));

        return patterns;
      }
      // 重写Replacement
      GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
        std::cout << "Define replacement for FuseMatMulAndAddPass" << std::endl;
        // 构建替换后的图
        auto replace_graph_builder = es::EsGraphBuilder("replacement");
        auto [r_a, r_b, r_c] = replace_graph_builder.CreateInputs<3>();
        auto alpha_const = replace_graph_builder.CreateScalar(1);
        auto beta_const = replace_graph_builder.CreateScalar(1);
        auto gemm = es::GEMM(r_a, r_b, r_c, alpha_const, beta_const);
        // 构建并重置图
        return replace_graph_builder.BuildAndReset({gemm});
      }
    };
    ```

3. 注册自定义融合Pass。

    ```c++
    // 使用REG_FUSION_PASS注册宏进行改图Pass注册，并指定被调用的阶段
    REG_FUSION_PASS(FuseMatMulAndAddPass).Stage(CustomPassStage::kBeforeInferShape);
    ```

## 如何使用自定义Pass

完成上述自定义Pass后，本节简单介绍如何把改图函数编译成动态库插件方式，以便注册的Pass在图编译阶段被框架调用。详细使用说明请参见[样例使用指导](https://gitcode.com/cann/ge/tree/master/examples/fusion_pass/1_fuse_matmul_add_pass)。

1. 把[开发示例](#开发示例)中的改图函数编译成仅以".so"结尾的动态库文件。
2. 编译成功后，执行**make install**命令，将上述".so"动态库文件安装到$\{INSTALL\_DIR\}/opp/vendors/_xxx_/custom\_fusion\_passes/目录下。（支持设置软链接的方式；".so"文件对执行用户，需要有可读权限）

    多个"$\{INSTALL\_DIR\}/opp/vendors/_xxx_"目录按照字母序排序后遍历寻找"custom\_fusion\_passes/"子目录，单个子目录内的".so"按照字母序加载，非".so"结尾的文件在加载时跳过。

    - 其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。
    - _xxx_：有且仅有一层自定义目录。
    - custom\_fusion\_passes：该目录下不能有子目录。

3. 支持但不限于如下几种入口编译模型文件：

    如果要查看上述自定义Pass有没有生效，在编译模型前，需要dump图进行查看：在执行之前设置DUMP\_GE\_GRAPH（详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)）环境变量，然后使用如下入口编译模型：

    - 使用ATC工具进行模型转换。ATC工具使用方法请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannAPIReference)。
    - [编译Graph为离线模型](../compile_and_run_graph/compile_graph_to_offline_model.md)。
    - [编译并运行Graph](../compile_and_run_graph/compile_and_run_graph.md)。

## 结果验证

请参见[样例使用指导](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/1_fuse_matmul_add_pass/cpp)\>程序运行\>查看运行结果。

设置了dump环境变量后，程序执行完毕，会在当前路径生成ge\_onnx\*.pbtxt等图文件，用户可以获取如下两张图（以指定Pass执行阶段在InferShape之前为例），然后使用Netron等可视化软件查看：

- ge\_onnx\_xxxx\_PreRunBegin.pbtxt：融合前的图
- ge\_onnx\_xxxx\_RunCustomPassBeforeInfershape.pbtxt：融合后的图

查看融合前的图结构为：

![](../figures/fusion_pass_image9.png)

通过自定义Pass修改后的图结构如下所示，可以看出MatMul+Add结构已经替换为单个GEMM节点。

![](../figures/fusion_pass_image10.png)
