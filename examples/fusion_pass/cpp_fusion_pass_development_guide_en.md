# C++ Fusion Pass Development Guide

This guide is for developers who want to write GE fusion passes in C++. It is recommended to first read the language-independent mechanism description: [Fusion Pattern Pass Mechanism](../../docs/zh/design/features/fusion_pattern_pass.md).

C++ passes are delivered as dynamic libraries. Developers implement a pass class, register it with GE, and compile it into a `.so`. When GE compiles a model, it loads the `.so` and executes the pass at a specified stage.

If you are still exploring patterns, it is recommended to use the [Python Fusion Pass Development Guide](python_fusion_pass_development_guide.md) for quick validation; migrate to C++ once the pattern is stable.

## 1. Which Pass to Choose

| Goal | Recommended Interface |
|------|----------|
| Match a fixed topology and replace it with another topology | `PatternFusionPass` |
| Match a specific operator type and decompose it into multiple operators | `DecomposePass` |

This guide covers `PatternFusionPass` first, then `DecomposePass`.

## 2. Minimal Example: Delete Add(x, 0)

Goal:

```text
x ----\
        Add ---- out      ==>      x ---- out
 0 ----/
```

The core C++ pass code consists of four parts:

1. Inherit `PatternFusionPass`.
2. `Patterns()` defines the structure to match.
3. `MeetRequirements()` checks if the constant is 0.
4. `Replacement()` returns the replacement structure.

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

A complete runnable example is available at [AddZeroPass C++ Example](pattern_base_pass/4_add_zero_pass/cpp/README.md).

## 3. Patterns: Define What to Find

`Patterns()` returns one or more patterns. Each pattern is a small graph.

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

This pattern represents:

```text
a ----\
        MatMul ----\
b ----/            Add ---- pattern output
c ----------------/
```

To support both `MatMul + Add` and `BatchMatMulV2 + Add`, create two patterns and add both to `patterns`.

When writing patterns, note:

- External inputs are declared with `CreateInput`.
- Tensors that will still be used externally after replacement must be outputs of the pattern.
- Input count for normal operators must match the real graph.
- Do not use control edges, subgraphs, or nodes with dynamic input/output counts in patterns.

## 4. MeetRequirements: Determine Whether to Replace

`Patterns()` only handles topology matching. If additional checks are needed after topology matching, write `MeetRequirements()`.

For example, after matching `Add(x, Const)`, verify that Const equals 0:

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

If no filtering is needed, this method can be omitted; it returns `true` by default.

## 5. Replacement: Define What to Replace With

`Replacement()` returns the replacement graph.

When deleting `Add(x, 0)`, the replacement graph has only one external input:

```cpp
GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
  auto builder = es::EsGraphBuilder("replacement");
  auto x = builder.CreateInput(0);
  return builder.BuildAndReset({x});
}
```

When fusing `MatMul + Add` into `GEMM`:

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

If the pass is registered after InferShape, shape information for new nodes in the replacement needs to be handled manually. Refer to existing examples for using `GeUtils::InferShape` when shape inference is needed for replacement.

## 6. CaptureTensor: Read Key Tensors in Pattern

When `MeetRequirements()` or `Replacement()` needs to know which real node corresponds to a intermediate tensor, capture it in the pattern.

```cpp
auto matmul = es::MatMul(a, b);
auto add = es::Add(matmul, c);
auto graph = builder.BuildAndReset({add});
auto pattern = std::make_unique<Pattern>(std::move(*graph));
pattern->CaptureTensor({*matmul.GetProducer(), 0});
patterns.emplace_back(std::move(pattern));
```

After successful matching, retrieve from `match_result`:

```cpp
NodeIo matmul_output;
if (match_result->GetCapturedTensor(0, matmul_output) != GRAPH_SUCCESS) {
  return false;
}
```

Refer to [capture tensor C++ example](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md).

## 7. PatternMatcherConfig: Put Simple Conditions in Matcher

If you want the matcher to directly check Const values or IR attributes, pass configuration to the `PatternFusionPass` constructor.

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

Common configurations:

| Configuration | Effect |
|------|------|
| `EnableConstValueMatch()` | Const values in pattern must match Const values in real graph |
| `EnableIrAttrMatch()` | IR attributes and values in pattern must match real graph |

If judgment requires floating-point tolerance, dtype normalization, or more complex logic, it is still recommended to put it in `MeetRequirements()`.

Refer to [PatternMatcherConfig C++ example](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md).

## 8. Register Execution Stage

Use `REG_FUSION_PASS` to register `PatternFusionPass`:

```cpp
REG_FUSION_PASS(AddZeroPass).Stage(CustomPassStage::kBeforeInferShape);
```

Common stages:

| C++ Enumeration | Usage Recommendation |
|----------|----------|
| `CustomPassStage::kBeforeInferShape` | Most commonly used. Replacement will go through unified shape inference |
| `CustomPassStage::kAfterInferShape` | Use when dependent on inferred shape; replacement must ensure shape information |
| `CustomPassStage::kAfterBuiltinFusionPass` | Execute after GE built-in fusion |
| `CustomPassStage::kAfterOriginGraphOptimize` | Execute after original graph optimization |

For initial development, use `kBeforeInferShape`.

## 9. Writing DecomposePass

If you want to decompose one node into multiple nodes, use `DecomposePass`.

Skeleton is as follows:

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
    // Read matched_node attributes to determine if decomposition is needed
    return true;
  }

  GraphUniqPtr Replacement(const GNode &matched_node) override {
    auto builder = es::EsGraphBuilder("replacement");
    // Construct subgraph for replacing matched_node
    ...
    return builder.BuildAndReset({output});
  }
};

REG_DECOMPOSE_PASS(MyDecomposePass, {"Conv2D"}).Stage(CustomPassStage::kAfterInferShape);
```

The second parameter of `REG_DECOMPOSE_PASS` is the list of operator types to match. GE will pass real nodes of these types to the pass, then `MeetRequirements()` makes further judgment.

Complete example see [DecomposePass C++ example](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/cpp/README.md).

## 10. Compilation and Running

Each example directory comes with `CMakeLists.txt`. General process is as follows.

Set CANN environment variables:

```bash
source ${ASCEND_PATH}/set_env.sh
```

Compile and install pass dynamic library:

```bash
mkdir build
cd build
cmake ..
make -j$(nproc) <target_name>
make install
```

CMake configuration not expanded in this document, use examples as template during development:

- [AddZeroPass CMakeLists.txt](pattern_base_pass/4_add_zero_pass/cpp/CMakeLists.txt)
- [MatMul+Add CMakeLists.txt](pattern_base_pass/1_fuse_matmul_add_pass/cpp/CMakeLists.txt)

If need to add new header file paths or link libraries, append in corresponding positions of example `CMakeLists.txt`, do not delete original configuration.

Offline compilation can use `atc` to trigger:

```bash
atc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model
```

Online scenario usually triggers GE compilation through `torch_forward.py` in examples.

## 11. Verification and Troubleshooting

Recommend enabling graph dump:

```bash
export DUMP_GE_GRAPH=1
```

Compare graphs before and after pass:

- `PreRunBegin`: Before pass execution.
- `RunCustomPass...`: After custom pass execution.

Common problems:

| Phenomenon | Possible Cause | Check Method |
|------|----------|----------|
| pass not executed | `.so` not installed to directory GE will load, or registration stage incorrect | Check installation path and registration macro |
| pattern not matched | Operator type, input count, output boundary inconsistent | Compare dump graph and `Patterns()` |
| matched but not replaced | `MeetRequirements()` returned `false` | Print matched node attributes |
| Graph abnormal after replacement | replacement output did not cover Tensor needed by external consumers | Go back to mechanism document boundary rules |

When more logs needed, can set:

```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1
export ASCEND_GLOBAL_LOG_LEVEL=0
```

When using `atc`, can add `--log=debug`.

## 12. Recommended Reading Order

1. [Fusion Pattern Pass Mechanism](../../docs/zh/design/features/fusion_pattern_pass.md)
2. [AddZeroPass C++ example](pattern_base_pass/4_add_zero_pass/cpp/README.md)
3. [MatMul+Add C++ example](pattern_base_pass/1_fuse_matmul_add_pass/cpp/README.md)
4. [capture tensor C++ example](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md)
5. [PatternMatcherConfig C++ example](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md)
6. [DecomposePass C++ example](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/cpp/README.md)
