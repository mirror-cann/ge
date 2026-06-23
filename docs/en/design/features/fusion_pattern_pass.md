# Fusion Pattern Pass Mechanism

## 1. What Problems Do These Passes Solve

When GE compiles a model, the input is a computation graph. Each node in the graph is an operator, and the edges between nodes represent which operator a Tensor flows from to which operator.

The goal of Fusion Pattern Pass is straightforward: **find a small structure in the graph that matches a rule, then replace that structure with another equivalent structure**.

For example, if there is a `MatMul + Add` segment in the graph:

```text
a ----\
        MatMul ----\
b ----/            Add ---- out
c ----------------/
```

If the target hardware can perform the same computation in one go with `GEMM`, it can be replaced with:

```text
a ----\
b ----- GEMM ---- out
c ----/
```

Another example: if the graph has `Add(x, 0)`, its output is equivalent to `x`, so it can be directly replaced with `x`. Such optimizations don't change the model semantics, but can reduce the number of operators, reduce intermediate Tensor reads/writes, or transform the graph into a form that's easier for the backend to execute efficiently.

GE provides two common interfaces:

| Interface | Applicable Scenarios | Intuitive Understanding |
|------|----------|----------|
| `PatternFusionPass` | Match a subgraph segment, then replace with another subgraph segment | "Find a local graph of this shape, then replace it entirely" |
| `DecomposePass` | Match a single operator, then replace with a subgraph composed of multiple operators | "Split a complex operator into several basic operators" |

Python and C++ implementations differ, but the underlying mechanism is the same: GE runs passes during the compilation phase to complete matching, filtering, replacement, and reconnection.

Development Guides:

- [Python Fusion Pass Development Guide](../../../../examples/fusion_pass/python_fusion_pass_development_guide.md)
- [C++ Fusion Pass Development Guide](../../../../examples/fusion_pass/cpp_fusion_pass_development_guide.md)


## 2. How a PatternFusionPass Executes

A single `PatternFusionPass` execution can be broken into five steps:

```text
Define pattern -> Match in target graph -> Filter by conditions -> Generate replacement -> Replace and reconnect edges
```

### 2.1 Define pattern

A `pattern` is a very small template graph used to express "what I want to find in the real graph".

Take `Add(x, 0)` as an example, the pattern only needs to express:

```text
External input x ----\
                       Add ---- pattern output
Constant input 0 ----/
```

Here "external input" is not a fixed real node, but a placeholder. During matching, GE will map the Tensors connected to the outside of this structure in the real graph to this placeholder.

### 2.2 Match in target graph

GE will search the real graph for isomorphic structures using the pattern. It can be understood as:

1. The types of regular operators in the pattern must match the operator types in the real graph.
2. Data edges in the pattern must have corresponding connections in the real graph.
3. The input and output boundaries of the pattern must be complete so the real graph remains connected after replacement.

After successful matching, GE generates a `MatchResult`. It records the real nodes, real edges, and actively captured Tensors in this match.

### 2.3 Filter with MeetRequirements

Topological matching only indicates "the shape looks right", not necessarily "can be replaced".

For example, `Add(x, Const)` topology can match all "input plus constant" structures, but only when the constant value equals 0 can it be replaced with `x`. This judgment should be placed in `MeetRequirements`.

`MeetRequirements` returns:

- `true`: This match satisfies the conditions and can proceed to replacement.
- `false`: Skip this match and continue searching for the next one.

### 2.4 Generate replacement graph with Replacement

`Replacement` returns another small graph used to replace the matched real subgraph.

If the pattern is `Add(x, 0)`, the replacement can just return input `x`:

```text
x ---- replacement output
```

If the pattern is `MatMul + Add`, the replacement can return `GEMM`:

```text
a ----\
b ----- GEMM ---- replacement output
b ----- GEMM ---- replacement output
c ----/
```

### 2.5 Replace and Reconnect

GE will delete the matched old subgraph, insert replacement, then reconnect external consumers to replacement according to pattern's declared input/output boundaries.

The most important here is "boundary". If boundaries are written incorrectly, even if pattern can match, it may not be able to safely replace.

## 3. Pattern Boundary Rules

Boundary rules are the most error-prone part when writing Pattern Pass. You can first remember one sentence:

**Pattern must completely explain where this subgraph receives external inputs, and which outputs still need to be passed to external use after replacement.**

### 3.1 Input Boundary

Any Tensor from outside the matching subgraph must be represented with input placeholder in pattern.

For example, to match `Add(x, 0)`, `x` comes from outside, constant `0` is created inside pattern:

```text
Data/Input ----\
                Add
Const(0) ------/
```

During matching, `Data/Input` can correspond to any upstream output in real graph.

### 3.2 Output Boundary

Any Tensor that will still be used by outside of subgraph after replacement must be pattern output.

For example, pattern is:

```text
X ---- A ---- out
```

If only `A`'s output is used by outside in real graph, then just declare `A` as pattern output.

If `X`'s output is also used by nodes outside pattern, must declare `X`'s output too:

```text
X ---- A ---- out0
|
out1
```

Otherwise after replacement, external nodes still want to use `X`'s output, but replacement doesn't provide output port for this Tensor, graph will disconnect. GE will try to reject such incomplete boundaries during matching phase.

### 3.3 Self-contained Constraint

For normal nodes inside pattern, if one of its outputs is not declared as pattern output, then all consumers of this output must also be inside pattern.

Can check with following questions:

- Will this Tensor still be used by external nodes after replacement?
- If yes, has it been declared as pattern output?
- If no, are its consumers all inside pattern?

### 3.4 Input Count Must Be Exact

Normal operator node's input count needs to match real graph. If an operator in real graph has 3 inputs, corresponding node in pattern must also have 3 inputs. Even if some inputs' sources are not cared about, still need to use input placeholders to fill.

### 3.5 Unsupported Pattern Contents

Pattern matching focuses on data topology, not suitable for expressing all graph structures. During development should avoid using in pattern:

| Content | Reason |
|---------|--------|
| Control edges | Pattern matcher doesn't match by control dependencies |
| Subgraphs | Doesn't support nested subgraph matching |
| Nodes with dynamic input count or dynamic output count | Cannot determine fixed input/output boundaries during matching |

### 3.6 Multi-output Pattern

A pattern can have multiple outputs. Multi-output is not "multiple patterns", but "one match exposes multiple output Tensors".

If just want to support multiple topologies, like simultaneously support `MatMul + Add` and `BatchMatMulV2 + Add`, should define multiple patterns.

## 4. Common Extension Points

### 4.1 CaptureTensor

Sometimes `MeetRequirements` or `Replacement` needs to read the real node corresponding to some intermediate Tensor in pattern, for example reading `MatMul`'s output description or attributes.

Then can capture this Tensor when defining pattern. After successful matching, retrieve it from `MatchResult` by capture order.

Python `@pattern` writing will automatically capture visited external inputs and `return`-ed pattern outputs; if want to read intermediate Tensors not returned as outputs, still need to use explicit composition and call `Pattern.capture_tensor`.

Typical uses:

- Check dtype、shape、format in `MeetRequirements`.
- Read original node attributes in `Replacement`, write to new node.
- Print hit location, convenient for confirming match results.

Reference samples:

- [C++ capture tensor sample](../../../../examples/fusion_pass/pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md)
- [Python capture tensor sample](../../../../examples/fusion_pass/pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/python/README.md)

### 4.2 PatternMatcherConfig

By default, pattern mainly matches topology and operator types. Some scenarios want matcher to be stricter, for example:

- Const values in pattern must match Const values in real graph.
- IR attributes and values declared in pattern must match real graph.

Then can enable `PatternMatcherConfig`.

Common switches:

| Configuration | Effect |
|---------------|--------|
| `EnableConstValueMatch` / `enable_const_value_match` | Match Const values |
| `EnableIrAttrMatch` / `enable_ir_attr_match` | Match IR attributes and values |

If judgment logic is simple, strict and stable, can put into matcher configuration; if need tolerance, dtype normalization, multiple condition combinations, usually putting in `MeetRequirements` is clearer.

Reference samples:

- [C++ PatternMatcherConfig sample](../../../../examples/fusion_pass/pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md)
- [Python PatternMatcherConfig sample](../../../../examples/fusion_pass/pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md)

## 5. How to Understand DecomposePass

`DecomposePass` is a more special replacement: it doesn't need to first define a pattern graph, but directly declares "I want to handle which operator types".

For example, to split `Conv2D` with `groups > 1` into multiple normal `Conv2D`:

```text
Grouped Conv2D
      |
      v
Split(input) + Split(filter) + Conv2D * N + Concat
```

Execution flow is:

```text
Find nodes by op type -> MeetRequirements judge if this node needs split -> Replacement generate replacement subgraph
```

`DecomposePass` suits:

- Target is single operator.
- Whether to replace mainly determined by this operator's attributes.
- Replacement will expand this operator into multiple operators.

Reference samples:

- [C++ DecomposePass sample](../../../../examples/fusion_pass/pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/cpp/README.md)
- [Python DecomposePass sample](../../../../examples/fusion_pass/pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/python/README.md)

## 6. Pass Execution Stage

Pass needs to specify execution stage when registering. Stage determines what graph state pass can see, also determines whether replacement needs to do shape derivation itself.

| Mechanism Stage | Python Enum | C++ Enum | Usage Recommendation |
|----------------|-------------|----------|----------------------|
| Before InferShape | `PassStage.BEFORE_INFER_SHAPE` | `CustomPassStage::kBeforeInferShape` | Most commonly used. Replacement will enter unified shape derivation flow afterwards |
| After InferShape | `PassStage.AFTER_INFER_SHAPE` | `CustomPassStage::kAfterInferShape` | Replacement needs to ensure output shape etc. info correct itself |
| After builtin fusion | `PassStage.AFTER_BUILTIN_FUSION_PASS` | `CustomPassStage::kAfterBuiltinFusionPass` | Use when want to handle after GE builtin fusion completes |
| After original graph optimization | `PassStage.AFTER_ORIGIN_GRAPH_OPTIMIZE` | `CustomPassStage::kAfterOriginGraphOptimize` | Use when want to append custom handling after original graph optimization ends |

Initial development suggests first choosing before InferShape stage. Only consider after InferShape stage when your judgment must depend on already derived shapes, or replacement itself will explicitly call shape derivation.

## 7. Python and C++ Relationship

Both Python and C++ passes will connect to GE's unified pass scheduling flow.

Main differences are in development experience and delivery method:

| Dimension | Python | C++ |
|-----------|--------|-----|
| Integration method | Load `.py` files or directories at runtime through `ASCEND_GE_PY_PASS_PATH` | Compile into `.so` then loaded by GE |
| Pattern writing | Recommend `@pattern` expression writing, also compatible with explicit composition | Use `EsGraphBuilder` explicit composition |
| Replacement writing | Can return expression, e.g. `return inputs[0]` | Return `GraphUniqPtr` |
| Suitable scenarios | Rapid development, business-side on-demand integration | Productized delivery, reuse C++ code, need stronger compile-time control |

If just adding one rule and verifying effect, suggest first write in Python. After rule stabilizes, if have delivery form or performance requirements, then consider C++ implementation.

## 8. Pre-development Checklist

Answer these questions before writing pass:

- Is what to handle "a subgraph segment" or "single operator"?
- If subgraph segment, are all external inputs of pattern declared?
- Are all outputs still used by outside after replacement declared?
- Is topology matching sufficient, or need to check dtype、shape、attributes or Const values in `MeetRequirements`?
- Is replacement input order consistent with pattern boundaries?
- Is registration stage appropriate? If running after InferShape, has replacement handled shape derivation?
- Can use `DUMP_GE_GRAPH=1` to compare graph before and after replacement, confirm rule actually took effect?