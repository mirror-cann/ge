# C++ 融合 Pass 开发指南

本文面向想用 C++ 编写 GE 融合 pass 的开发者。建议先阅读语言无关的机制说明：[融合 Pattern Pass 机制](../../docs/architecture/features/fusion_pattern_pass.md)。

C++ pass 的交付形态是动态库。开发者实现 pass 类，把它注册到 GE，然后编译成 `.so`。GE 编译模型时加载该 `.so`，在指定阶段执行 pass。

如果你还在探索规则，建议先用 [Python 融合 Pass 开发指南](python_fusion_pass_development_guide.md) 快速验证；规则稳定后再迁移到 C++。

## 1. 选择哪种 pass

| 目标 | 推荐接口 |
|------|----------|
| 匹配一段固定拓扑，再替换成另一段拓扑 | `PatternFusionPass` |
| 匹配某个算子类型，再把单个算子拆成多个算子 | `DecomposePass` |

本文先讲 `PatternFusionPass`，再讲 `DecomposePass`。

## 2. 最小示例：删除 Add(x, 0)

目标：

```text
x ----\
       Add ---- out      ==>      x ---- out
0 ----/
```

C++ pass 的核心代码由四部分组成：

1. 继承 `PatternFusionPass`。
2. `Patterns()` 定义要匹配的结构。
3. `MeetRequirements()` 判断常量是否为 0。
4. `Replacement()` 返回替换结构。

```cpp
#include <cmath>
#include <cstdint>
#include <iostream>
#include "es_all_ops.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"

using namespace ge;
using namespace ge::fusion;

class AddZeroPass : public PatternFusionPass {
 protected:
  std::vector<PatternUniqPtr> Patterns() override {
    std::vector<PatternUniqPtr> patterns;
    auto builder = es::EsGraphBuilder("add_zero_pattern");
    auto x = builder.CreateInput(0);
    auto zero = es::Const(builder);
    auto add = es::Add(x, zero);
    auto graph = builder.BuildAndReset({add});
    patterns.emplace_back(std::make_unique<Pattern>(std::move(*graph)));
    return patterns;
  }

  bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
    for (const auto &node : match_result->GetMatchedNodes()) {
      AscendString type;
      node.GetType(type);
      if (type != "Const") {
        continue;
      }
      Tensor value;
      if (node.GetAttr("value", value) != GRAPH_SUCCESS) {
        return false;
      }
      return IsZero(value);
    }
    return false;
  }

  GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
    auto builder = es::EsGraphBuilder("add_zero_replacement");
    auto x = builder.CreateInput(0);
    return builder.BuildAndReset({x});
  }

 private:
  bool IsZero(const Tensor &tensor) const {
    switch (tensor.GetTensorDesc().GetDataType()) {
      case DT_FLOAT:
        return std::fabs(*reinterpret_cast<const float *>(tensor.GetData())) < 1e-6;
      case DT_DOUBLE:
        return std::fabs(*reinterpret_cast<const double *>(tensor.GetData())) < 1e-15;
      case DT_INT32:
        return *reinterpret_cast<const int32_t *>(tensor.GetData()) == 0;
      default:
        return false;
    }
  }
};

REG_FUSION_PASS(AddZeroPass).Stage(CustomPassStage::kBeforeInferShape);
```

完整可运行样例见 [AddZeroPass C++ 样例](pattern_base_pass/4_add_zero_pass/cpp/README.md)。

## 3. Patterns：定义要找什么

`Patterns()` 返回一个或多个 pattern。每个 pattern 都是一张小图。

```cpp
std::vector<PatternUniqPtr> Patterns() override {
  std::vector<PatternUniqPtr> patterns;

  auto builder = es::EsGraphBuilder("pattern");
  auto a = builder.CreateInput(0);
  auto b = builder.CreateInput(1);
  auto c = builder.CreateInput(2);
  auto matmul = es::MatMul(a, b);
  auto add = es::Add(matmul, c);
  auto graph = builder.BuildAndReset({add});

  patterns.emplace_back(std::make_unique<Pattern>(std::move(*graph)));
  return patterns;
}
```

这段 pattern 表示：

```text
a ----\
       MatMul ----\
b ----/            Add ---- pattern 输出
c ----------------/
```

如果要同时支持 `MatMul + Add` 和 `BatchMatMulV2 + Add`，就创建两个 pattern 并都放进 `patterns`。

写 pattern 时注意：

- 外部输入用 `CreateInput` 声明。
- replacement 后仍会被外部使用的 Tensor 必须作为 pattern 输出。
- 普通算子的输入个数要和真实图一致。
- 不要在 pattern 中使用控制边、子图或动态输入输出个数的节点。

## 4. MeetRequirements：判断能不能替换

`Patterns()` 只负责拓扑匹配。拓扑命中后，如果还要检查条件，就写 `MeetRequirements()`。

例如匹配 `Add(x, Const)` 后，还要确认 Const 值等于 0：

```cpp
bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
  for (const auto &node : match_result->GetMatchedNodes()) {
    AscendString type;
    node.GetType(type);
    if (type != "Const") {
      continue;
    }
    Tensor value;
    if (node.GetAttr("value", value) != GRAPH_SUCCESS) {
      return false;
    }
    return IsZero(value);
  }
  return false;
}
```

如果不需要过滤，可以不重写这个方法，默认返回 `true`。

## 5. Replacement：定义替换成什么

`Replacement()` 返回替换图。

删除 `Add(x, 0)` 时，替换图只有一个外部输入：

```cpp
GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
  auto builder = es::EsGraphBuilder("replacement");
  auto x = builder.CreateInput(0);
  return builder.BuildAndReset({x});
}
```

把 `MatMul + Add` 融合成 `GEMM` 时：

```cpp
GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
  auto builder = es::EsGraphBuilder("replacement");
  auto a = builder.CreateInput(0);
  auto b = builder.CreateInput(1);
  auto c = builder.CreateInput(2);
  auto alpha = builder.CreateScalar(1);
  auto beta = builder.CreateScalar(1);
  auto gemm = es::GEMM(a, b, c, alpha, beta);
  return builder.BuildAndReset({gemm});
}
```

如果 pass 注册在 InferShape 后，replacement 中新建节点的 shape 信息需要自行处理。需要对 replacement 做 shape 推导时，可参考现有样例中对 `GeUtils::InferShape` 的使用。

## 6. CaptureTensor：读取 pattern 中的关键 Tensor

当 `MeetRequirements()` 或 `Replacement()` 需要知道某个中间 Tensor 对应的真实节点时，可以在 pattern 中捕获它。

```cpp
auto matmul = es::MatMul(a, b);
auto add = es::Add(matmul, c);
auto graph = builder.BuildAndReset({add});
auto pattern = std::make_unique<Pattern>(std::move(*graph));
pattern->CaptureTensor({*matmul.GetProducer(), 0});
patterns.emplace_back(std::move(pattern));
```

匹配成功后，在 `match_result` 中取回：

```cpp
NodeIo matmul_output;
if (match_result->GetCapturedTensor(0, matmul_output) != GRAPH_SUCCESS) {
  return false;
}
```

参考 [capture tensor C++ 样例](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md)。

## 7. PatternMatcherConfig：把简单条件前置到 matcher

如果希望 matcher 直接检查 Const 值或 IR 属性，可以给 `PatternFusionPass` 构造函数传入配置。

```cpp
class MatmulAddFusionPass : public PatternFusionPass {
 public:
  MatmulAddFusionPass()
      : PatternFusionPass(PatternMatcherConfigBuilder()
                              .EnableConstValueMatch()
                              .EnableIrAttrMatch()
                              .Build()) {}
};
```

常用配置：

| 配置 | 作用 |
|------|------|
| `EnableConstValueMatch()` | pattern 中 Const 值要和真实图中的 Const 值一致 |
| `EnableIrAttrMatch()` | pattern 中 IR 属性和值要和真实图一致 |

如果判断需要浮点容差、dtype 归一化或更复杂逻辑，仍建议放在 `MeetRequirements()` 中。

参考 [PatternMatcherConfig C++ 样例](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md)。

## 8. 注册执行阶段

使用 `REG_FUSION_PASS` 注册 `PatternFusionPass`：

```cpp
REG_FUSION_PASS(AddZeroPass).Stage(CustomPassStage::kBeforeInferShape);
```

常用阶段：

| C++ 枚举 | 使用建议 |
|----------|----------|
| `CustomPassStage::kBeforeInferShape` | 最常用。replacement 后续会进入统一 shape 推导 |
| `CustomPassStage::kAfterInferShape` | 需要依赖已推导 shape 时使用，replacement 要自行保证 shape 信息 |
| `CustomPassStage::kAfterBuiltinFusionPass` | GE 内置融合完成后再执行 |
| `CustomPassStage::kAfterOriginGraphOptimize` | 原图优化完成后再执行 |

初次开发建议先使用 `kBeforeInferShape`。

## 9. 写 DecomposePass

如果要把一个节点拆成多个节点，用 `DecomposePass`。

骨架如下：

```cpp
#include "ge/fusion/pass/decompose_pass.h"
#include "es_all_ops.h"

using namespace ge;
using namespace ge::fusion;

class MyDecomposePass : public DecomposePass {
 public:
  explicit MyDecomposePass(const std::vector<AscendString> &op_types)
      : DecomposePass(op_types) {}

 protected:
  bool MeetRequirements(const GNode &matched_node) override {
    // 读取 matched_node 的属性，判断是否需要分解
    return true;
  }

  GraphUniqPtr Replacement(const GNode &matched_node) override {
    auto builder = es::EsGraphBuilder("replacement");
    // 构造用于替换 matched_node 的子图
    ...
    return builder.BuildAndReset({output});
  }
};

REG_DECOMPOSE_PASS(MyDecomposePass, {"Conv2D"}).Stage(CustomPassStage::kAfterInferShape);
```

`REG_DECOMPOSE_PASS` 的第二个参数是要匹配的算子类型列表。GE 会把这些类型的真实节点交给 pass，再由 `MeetRequirements()` 做进一步判断。

完整样例见 [DecomposePass C++ 样例](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/cpp/README.md)。

## 10. 编译和运行

每个样例目录都带有 `CMakeLists.txt`。一般流程如下。

设置 CANN 环境变量：

```bash
source ${ASCEND_PATH}/set_env.sh
```

编译并安装 pass 动态库：

```bash
mkdir build
cd build
cmake ..
make -j$(nproc) <target_name>
make install
```

CMake 配置不在本文展开，开发时直接以样例为模板：

- [AddZeroPass CMakeLists.txt](pattern_base_pass/4_add_zero_pass/cpp/CMakeLists.txt)
- [MatMul+Add CMakeLists.txt](pattern_base_pass/1_fuse_matmul_add_pass/cpp/CMakeLists.txt)

如果需要新增头文件路径或链接库，在样例 `CMakeLists.txt` 对应位置追加即可，不要删除原有配置。

离线编译可用 `atc` 触发：

```bash
atc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model
```

在线场景通常由样例中的 `torch_forward.py` 触发 GE 编译。

## 11. 验证和排查

建议打开图 dump：

```bash
export DUMP_GE_GRAPH=1
```

对比 pass 前后的图：

- `PreRunBegin`：pass 执行前。
- `RunCustomPass...`：自定义 pass 执行后。

常见问题：

| 现象 | 可能原因 | 检查方式 |
|------|----------|----------|
| pass 没执行 | `.so` 没安装到 GE 会加载的目录，或注册阶段不对 | 检查安装路径和注册宏 |
| pattern 不命中 | 算子类型、输入个数、输出边界不一致 | 对比 dump 图和 `Patterns()` |
| 命中但不替换 | `MeetRequirements()` 返回 `false` | 打印命中节点属性 |
| 替换后图异常 | replacement 输出没有覆盖外部消费者需要的 Tensor | 回看机制文档的边界规则 |

需要更多日志时可设置：

```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1
export ASCEND_GLOBAL_LOG_LEVEL=0
```

使用 `atc` 时可增加 `--log=debug`。

## 12. 推荐阅读顺序

1. [融合 Pattern Pass 机制](../../docs/architecture/features/fusion_pattern_pass.md)
2. [AddZeroPass C++ 样例](pattern_base_pass/4_add_zero_pass/cpp/README.md)
3. [MatMul+Add C++ 样例](pattern_base_pass/1_fuse_matmul_add_pass/cpp/README.md)
4. [capture tensor C++ 样例](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md)
5. [PatternMatcherConfig C++ 样例](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md)
6. [DecomposePass C++ 样例](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/cpp/README.md)
