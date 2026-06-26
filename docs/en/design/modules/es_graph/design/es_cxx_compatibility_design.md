# Historical Prototype Library Design Document (ES Scenario)

## 0. Reading Guide

- This document focuses on the ES scenario: how `gen_esb` consumes historical prototype data and generates ES APIs (with emphasis on Chapter 4 overload generation and disambiguation).
- The **protocol-level content** of the historical prototype library (positioning/directories/format/generation & release/evolution rules), see [protocol document](history_op_registry_protocol.md).

## 0.1 Terminology (Minimum Set)

- Historical Prototype Library: Structured data and protocols for archiving operator IR prototypes across versions.
- Structured Data: Three types of files (`index.json` / `metadata.json` / `operators.json`) and their directory organization.
- Producer: Tool that generates structured data (currently fixed as `gen_esb --extract-history`).
- Consumer: Any tool that reads structured data (currently mainly `gen_esb`, can be extended to other toolchains in the future).
- One-year Window/Compatibility Cycle: The version range that consumers select for comparison/generation based on version metadata.
- Ops Run Package: Ops runtime package form after installation (if "run package" appears below, it refers to Ops run package).

## 1. Overview

### 1.1 Background

In ES (Eager Style) graph building, to ensure backward API compatibility for C++ ES APIs, the generation tool needs to understand the IR prototype evolution across different commercial release versions. The historical prototype library is responsible for accumulating and providing this historical prototype data for `gen_esb` to compare differences and generate overloaded interfaces during the generation phase.

Note:
- `gen_esb` is a tool in the ES scenario: currently both the main consumer of historical prototype data and the generation tool for structured data (`--extract-history`). No new generation tools will be provided in the short term.
- The historical prototype library protocol layer is not bound to ES: it can be reused by other consumers in the future. This document only describes the consumption and generation strategies for the ES scenario.


## 2. Module Architecture and Responsibility Division

### 2.1 Module Division

The historical prototype library system consists of two core modules:

1. **Historical Prototype Library Module (Ops Component, Ops Package)**: Responsible for storing and managing historical prototype data
2. **Consumption and Generation Tool Module (gen_esb, GE Repository, GE Component, GE Package)**:
  - **Consumer Role**: Responsible for reading historical data and generating C++ overloaded interfaces
  - **Producer Role**: Responsible for generating historical prototype library data (reusing `gen_esb --extract-history`)

### 2.2 Historical Prototype Library Module Responsibilities

In one sentence: The responsibility of the historical prototype library module is to **store and provide historical prototype structured data in file system form**.

Boundaries:
- Only provides data and metadata (version index, operator prototypes, etc.), does not provide business logic output
- Does not perform disambiguation detection/compatibility judgment/overload generation/code generation

Protocol details (directories/files/fields) see [protocol document](history_op_registry_protocol.md).

### 2.3 gen_esb Tool Responsibilities

**The gen_esb tool is responsible for all logic related to code generation**.

#### 2.3.1 Consumer Role

#### 2.3.1.1 Data Reading

- Read historical version IR definitions from the historical prototype library within the compatibility cycle, and parse JSON-formatted historical prototype data

#### 2.3.1.2 Version Comparison Analysis

- Compare IR definition differences between current version (obtained directly from prototype .so) and historical versions, identify newly added optional inputs, attributes, etc.
- **Changes to default operator prototypes are assumed to be compatible**, no compatibility check is performed (GE's flow already has standard compatibility check flow, and operator prototype change compatibility itself is a basic requirement, so no additional checks)

#### 2.3.1.3 Overload Generation

- Generate legal overload signatures based on version differences while adding necessary safeguards, see [chapter](#4-overload-generation-and-disambiguation)

#### 2.3.1.4 Code Output

- Generate C++ interface header files (containing multiple overload versions)
- Generate C interfaces (no overload, generate prototypes corresponding to current version)
- Generate Python interfaces (no overload, generate prototypes corresponding to current version)

#### 2.3.2 Producer Role

#### 2.3.2.1 Historical Prototype Library Data Generation

- **Reuse gen_esb tool**: Before commercial release version publication, use `gen_esb` tool to generate current version prototype data
- Extract IR definitions from current version prototype .so files
- Generate JSON-formatted historical prototype data
- Output to historical prototype library directory structure


### 2.4 Inter-Module Interfaces

#### 2.4.1 Interfaces Provided by Historical Prototype Library

The historical prototype library module provides data through **file system interfaces**:

##### 2.4.1.1 Package File Directory Structure
Protocol and directory/data format details in `history_op_registry_protocol.md`. This document only uses its file system interfaces and key filenames:
- `index.json`: Version list (including release dates, etc.)
- `registry/<ver>/operators.json`: Operator prototype definitions for that version
- `registry/<ver>/metadata.json`: Version metadata (e.g., release date, branch name)

#### 2.4.2 Usage of gen_esb Tool

**Consuming Historical Prototypes for ES API Code Generation**:
```bash
gen_esb \
  --history-registry /path/to/registry/math \        # Historical prototype library package directory
  --output-dir /path/to/output \                # Output directory
  --release-version 8.0.RC2                     # Current version number, used to query compatibility cycle
```

The generation and publication of historical prototype library structured data belongs to protocol-side content, see "Generation and Publication" and "Appendix JSON Examples" in [protocol document](history_op_registry_protocol.md).

#### 2.4.3 Usage of generate_es_package Function

[generate_es_package](../../../../user-guides/es_graph/tools/generate_es_package_cmake_readme.md) is a cmake function wrapping gen_esb, with current parameters as follows:

| Parameter | Required | Description | Example |
|------|--------|------|------|
| `ES_LINKABLE_AND_ALL_TARGET` | Required | Library target name exposed externally | `es_math`, `es_nn`, `es_cv` |
| `OPP_PROTO_TARGET` | Required | CMake target name of operator prototype library | `opgraph_math`, `opgraph_nn` |
| `OUTPUT_PATH` | Required | Root directory for product output | `${CMAKE_BINARY_DIR}/output` |
| `EXCLUDE_OPS` | Optional  | Operators to exclude from generation | `Add,Conv2D`|


**Simplified Approach**

The engineering team defines some global cmake variables, and the generate_es_package function internally obtains and processes them directly, without needing generate_es_package to add new parameters;

The benefits of this approach are:
ops components have less awareness, combined with the current ops multi-repository and multi-package background, processing inside the generate_es_package function can avoid each repository needing to adapt;


**Alternative Approach**

1. Add the following parameters:

| Parameter | Required | Description | Example |
|------|--------|------|------|
| `HISTORY_REGISTRY`| Optional  | Historical prototype structured data directory on build environment | `/path/to/registry/math` |
| `EXTRACT_HISTORY` | Optional  | Generate historical prototype structured data, default not generate | `ON`|
| `RELEASE_VERSION` | Optional  | Current version number | `8.0.RC2`|

2. Caller (Ops) passes corresponding parameters

**Generate ES API Code**:
Additionally pass HISTORY_REGISTRY (optional, if there are historical prototypes), RELEASE_VERSION (optional, if there are historical prototypes)

**Generate ES API && Historical Prototype Library Data**:
Additionally pass HISTORY_REGISTRY (optional, if there are historical prototypes), RELEASE_VERSION (required, version number corresponding to current historical prototype data), EXTRACT_HISTORY (required, inform that historical prototype structured data generation mode is enabled)

**Recommended to use simplified approach**


#### 2.4.4 Other Required Information

The following information, if explicit passing is needed, requires processing corresponding parameters inside gen_esb and generate_es_package

release_date: commercial release time, branch_name: branch name

### 2.5 Responsibility Boundary Summary

| Function | Historical Prototype Library Module | gen_esb Tool |
|-----|--------------|-------------|
| Store historical prototype data | Responsible | Not responsible |
| Provide data query interface | Responsible (file system form) | Not responsible |
| Generate historical prototype library data | Not responsible | Responsible |
| Read historical data | Not responsible | Responsible |
| Version comparison analysis | Not responsible | Responsible |
| Compatibility detection | Not responsible | Not responsible (default compatible) |
| Disambiguation detection | Not responsible | Responsible |
| Overload generation | Not responsible | Responsible |
| Code generation | Not responsible | Responsible |

**Core Principles**:
- **Historical Prototype Library = Data Storage + Data Provision** (pure data layer, as part of ops run package)
- **gen_esb = Data Generation + Data Reading + Business Logic + Code Generation** (business logic layer)

## 3. Data Flow Diagram

The "historical prototype data generation/merge/package publication" process for building Ops packages belongs to protocol-side content, see "Generation and Publication" in [protocol document](history_op_registry_protocol.md).

**Component Collaboration Flow Diagram (ES Scenario)**

The collaboration between engineering team, operators, and GE is as follows:

![Collaboration Flow Diagram](./figures/process_view.svg)


Specifically:

**Initial Build of Commercial Release Version**

The engineering team passes information (via compilation macros), telling GE to complete logic parts 1 and 2:

1. Based on historical prototype structured data and prototypes in current prototype .so, generate operator-granularity C++ overload interfaces

2. Historical prototype library structured data generation (to simplify ops processing logic in step 3, this step generates current version historical prototype library structured data and then merges existing historical prototype library structured data together to output to specified directory)

3. Ops component uniformly packages products from 1 and 2, completing ops package build


**Non-commercial release build or commercial release version daily build**

Complete logic parts 1 and 3 from the above commercial release flow



## 4. Overload Generation and Disambiguation

### 4.1 Problem Root Cause

1. C++ requires default parameters to be at the end of parameter list, optional parameters of basic types will have overload disambiguation
2. `EsTensorLike` supports implicit conversion from scalar types (`int64_t`, `float`), making scalar values simultaneously match `EsTensorLike` parameters and `int64_t` parameters, producing disambiguation.


### 4.2 Solution

#### 4.2.1 Design Constraints (User-side Semantic Conventions)

This chapter's goal: Without introducing "version namespace", make historical overload interfaces compilable, non-disambiguous, and try to maintain stable user call habits.

Core facts (from `EsTensorLike` definition):
- `EsTensorLike(const std::nullptr_t)`: Can use `nullptr` to express "this input has no edge" (optional input semantics)
- `EsTensorLike(const int64_t/float/vector<...>)`: Scalars/vectors will be treated as constant inputs (implicit construction)

Core constraints (user-side semantic conventions, oriented toward "retain v1, generate multi-version overloads" strategy):
- When adding optional input `xo2`:
  - **Not using xo2 (no edge)**: Fixed use of v1 form (e.g., `Foo(x, xo1[, attr...])`), ensuring maximum forward compatibility
  - **Using xo2 (edge exists)**: Use v2 form (e.g., `Foo(x, xo1, xo2[, attr...])`)
- To avoid `Foo(x, xo1)` having disambiguation when v1/v2 both exist: v2's `xo2` must be **required parameter** (must not provide `=nullptr` default value).
- Also generate safeguard: Give deprecated+delete explicit diagnosis for "passing `nullptr` at v2's `xo2` position", prompting user "for no edge please use v1".
- When operator simultaneously has "attribute parameters that can be occupied by scalar literals" (like `int64_t a=0`):
  - At this time `Foo(x, xo1, 0)` is fixed interpreted as attribute (like `a=0`), not allowed to treat `0` as new input `xo2`
  - To pass scalar/vector as `xo2`, must first construct as `Const/Scalar/Vector` (like `EsGraphBuilder::CreateScalar/CreateVector/CreateConst`) then pass

#### 4.2.2 Solution A: Same Namespace, Multi-version Overload (Recommended)

Core goal of Solution A: Within same namespace, through a set of compilable and non-disambiguous overload collection, balance backward compatibility and forward compatibility.

##### 4.2.2.1 A1: New Input Still Uses `EsTensorLike`, but v2 `xo2` Required + `nullptr` Safeguard

Applicable scenario: New optional input `xo2` will not produce "scalar literal disambiguation" with existing parameters (e.g., attribute is `const char*` / `std::string` etc., will not conflict with `EsTensorLike(int64_t)`).

Approach:
- Retain v1: Used to express `xo2` has no edge (most forward compatible call form)
- v2: Add `xo2`, but generate `xo2` as required parameter (no default value), used to express `xo2` has edge
- Safeguard: Additionally generate `std::nullptr_t` deprecated+delete overload, prompting "xo2 no edge please use v1"

Example (`const char*` default attribute scenario):

```cpp
// v1: xo2 has no edge
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1,
                            const char *mode = "xx");

// v2: xo2 has edge (xo2 required, avoid Foo(x, xo1) disambiguation)
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1,
                            const EsTensorLike &xo2, const char *mode = "xx");

// Safeguard: If xo2 has no edge, please directly call v1
[[deprecated("xo2 no edge please call v1: Foo(x, xo1[, mode]); if xo2 needs edge please pass valid Tensor/Const.")]]
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1,
                            std::nullptr_t /*xo2*/, const char *mode = "xx") = delete;
```

##### 4.2.2.2 A2: Switch to `TensorHolder` when Scalar Literal Disambiguation (Fallback Disambiguation)

Trigger condition (typical): New optional input `xo2` coexists with "scalar attribute default parameter", will cause `Foo(x, xo1, 0)` type calls to have disambiguation risk (`0` could be treated as scalar constant for `xo2`, or could be treated as attribute).

Approach: Change new input parameter type to `TensorHolder` without default value; if user needs to pass scalar/vector as `xo2`, must first `CreateScalar/CreateVector/CreateConst`.

```cpp
// v1 version (retained, for backward compatibility)
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1, int64_t a=0);

// v2 version (fallback: new input xo2 becomes required + TensorHolder disambiguation)
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1,
                            const TensorHolder &xo2, int64_t a=0, int64_t b=0);
```

#### 4.2.3 Solution B: Version Namespace (Alternative)

Solution B puts different versions' APIs in different namespaces (e.g., `ge::es` and `ge::es::v2`), thus avoiding same-name overload collection expansion and disambiguation:

```cpp
// v1 version
namespace ge::es {
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1, const char *mode = "xx");
}

// v2 version (new namespace)
namespace ge::es::v2 {
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1,
                            const EsTensorLike &xo2 = nullptr, const char *mode = "xx");
}
```

#### 4.2.4 Solution Comparison and Trade-off

| Dimension | Solution A (Same Namespace, Multi-version Overload) | Solution B (Version Namespace) |
|---|---|---|
| Overload disambiguation risk | Low: A1 default + A2 TensorHolder fallback disambiguation when needed | Low: Namespace isolation |
| User call habit stability | High: `Foo(x, xo1[, attr...])` always represents "no edge" | Medium: Caller needs to explicitly choose namespace |
| Forward compatibility guidance | Can use deprecated+delete to force user "no edge go v1" | Mainly relies on documentation norms |
| Runnable Dump (C++) | Low cost: dump only needs to judge "whether edge exists" to choose v1/v2 form | High cost: dump needs to determine namespace/version, and handle dependencies |
| Dynamic library ABI | Still cannot guarantee (namespace doesn't affect underlying C ABI) | Still cannot guarantee |

Overall trade-off: This design chooses Solution A.

#### 4.2.5 Overload Generation Strategy (Solution A: A1/A2)

This subsection provides implementable generation rules (for `gen_esb`), goal: on "one-year window" version chain, generate a **minimal and non-disambiguous** C++ interface collection.

Version chain explanation:
- Version chain is sorted by release time in version metadata (e.g., `metadata.json.release_date`) to get `V0 < ... < Vn`; one-year window filtering strategy is calculated by `gen_esb` based on current version number and release date.

Terminology definition:
- Extraction existence: For a version's operator prototype, judge whether a certain input/attribute "exists" in IR prototype (based on whether IR prototype declares that name; optional/dynamic inputs are also treated as existing).
- New addition point: When comparing adjacent two versions, satisfies "new version exists, old version doesn't exist" input/attribute.
- First appearance: In version sequence (from old to new), the version where a certain input/attribute first changes from "non-existent" to "existent".

Note (new addition point vs first appearance):
- If IR prototype only has "monotonic compatible changes" within one-year window (only adding optional inputs/attributes with defaults), then adjacent versions' "new addition points" are equivalent to "first appearance" on version chain.
- But we also need to cover future possible compatible changes (e.g., required→optional), such changes don't necessarily manifest as exists changing from 0→1; therefore implementation still retains
  distinction between "new addition point (exists change)" and "first trigger generation (may come from exists/required/default change)".

Generation method (each operator executes independently; for guiding `gen_esb` implementation):
- Input: One-year window version chain `V0 < V1 < ... < Vn` IR prototypes (inputs/attrs and default values).
- Output: A **minimal, non-disambiguous, with safeguard** C++ interface signature collection (including Runnable Dump generation constraints).

Phase 0: Extract existence matrix (see Chapter 8 Appendix "Existence Matrix and Disambiguation Detection Examples")
- For each version `Vi` extract `exists_input[Vi][name]`, `exists_attr[Vi][name]`.
- For adjacent version pairs `(Vi-1, Vi)` calculate new addition points `new_inputs(Vi)`, `new_attrs(Vi)`.

Phase 1: Candidate signature generation (generate first, then judge; if Phase 2 fails go back here to converge; don't enumerate cases here)
- Only add optional attribute (with default value): Don't add overload; just append default attribute to **latest interface** parameter end.
- Add optional input: First judge whether **can safely merge to single signature (A0)**, otherwise enter "retain v1 convergence chain (try 0→A1→A2)":
  - Safe merge (A0) judgment (when all satisfied can merge, and no longer retain v1):
    - New input appended to end, and using `= nullptr` can accurately express "no edge" semantics
    - No "scalar literal might be mistakenly treated as new input" risk (typical trigger: simultaneously exists `int64_t a=0` type scalar default attribute position)
    - Don't need "retain v1 + safeguard" to constrain dump/user writing (e.g., don't need to prohibit `Foo(..., nullptr)` placeholder)
    - After passing: Generate single signature `Foo(..., xo2=nullptr, ...)`, no longer generate v1; this scenario doesn't involve overload disambiguation judgment (Phase 2 can skip).
  - Not satisfying safe merge: Enter convergence chain (retain v1):
    - Try 0 (semantic priority): Generate v1, simultaneously let v2's new input `xo2` stay optional (`xo2 = nullptr`); if Phase 2 judges v1/v2 has potential disambiguation, then abandon and upgrade to A1.
    - A1 (convergence 1): Retain v1 to express "no edge"; v2 new input `xo2` uses `EsTensorLike` and required (no default value), express "edge exists"; and generate `nullptr` safeguard (see 4.3).
    - A2 (convergence 2/fallback): If A1 still might have overload disambiguation with scalar literals, then switch v2's `xo2` to `TensorHolder` required, and require dump/user explicitly `CreateScalar/CreateVector/CreateConst`.

Phase 2: Disambiguation judgment (generation side must execute; if fails go back to Phase 1 to converge)
- Gate1 (candidate pair filtering): For any two candidate interfaces `f/g`, calculate callable argument count ranges `Rf=[req_f, tot_f]`, `Rg=[req_g, tot_g]` (`req`=required parameter count). If `Rf ∩ Rg` is empty, then impossible disambiguation; otherwise enter Gate2.
- Gate2 (typical argument set check, conservative judgment): In calls where `k ∈ (Rf ∩ Rg)`, check whether there exists some **typical argument** that can simultaneously match `f/g`'s parameter types at same position. Suggest at least covering two categories:
  - Can implicitly convert to `EsTensorLike`: `nullptr`, `0`, `0.0` (corresponding to `EsTensorLike(std::nullptr_t/int64_t/float)`).
  - Cannot implicitly convert to `EsTensorLike`: `"xx"` (`const char*`, used to verify "attribute string" won't compete with input position).
  If there exists typical argument that can simultaneously match, then judge as potential disambiguation: prohibit this combination; go back to Phase 1 to upgrade strategy (try 0→A1→A2), or adjust signature collection. Note: This check is engineering conservative approximation, doesn't pursue complete equivalence to C++ overload resolution rules.

Phase 3: Safeguard and export constraint landing
- If final collection contains "v2 new input required" overload (A1/A2), then automatically generate `std::nullptr_t` deprecated+delete safeguard overload (see 4.3), and require Runnable Dump not to output `(..., nullptr, ...)` placeholder form.
- A0 (single signature and new input default `= nullptr`) doesn't generate above `nullptr` safeguard; Runnable Dump can omit this input to utilize default value.

Minimal examples (two scenario comparison):

A) Only add optional input, and no scalar literal disambiguation (default solution: v2 make xo2 required + safeguard)
- v1: `Foo(x, xo1[, mode])` (xo2 has no edge)
- v2: Add optional input `xo2`
Generate: Retain v1 + generate v2 (xo2 required) + generate nullptr safeguard
- `Foo(x, xo1[, mode])`
- `Foo(x, xo1, xo2[, mode])`
- `Foo(x, xo1, nullptr[, mode]) = delete` (deprecated prompt use v1)

B) Add optional input + exists scalar attribute default parameter (default solution will conflict with literal, trigger fallback solution A)
- v1: `Foo(x, xo1, a=0)`
- v2: Add optional input `xo2`
Generate: Retain v1 + add v2 overload (`TensorHolder` disambiguation)
- `Foo(x, xo1, a=0)`
- `Foo(x, xo1, TensorHolder xo2, a=0, b=0)`

### 4.3 Safeguard Mechanism (Solution A)

Safeguard goal: Mainly used to prevent "placeholder writing that breaks forward compatibility", and give explicit migration prompt.

Note: As long as new input is required parameter in v2 (regardless of whether type is `EsTensorLike` or `TensorHolder`), suggest generating `std::nullptr_t` deprecated+delete safeguard overload, used to intercept user mistakenly treating `nullptr` as "no edge" writing, and prompt fallback to v1.
Supplement: A0 scenario (merge to single signature and new input default `= nullptr`) doesn't need to generate `nullptr` safeguard overload.

#### 4.3.1 `nullptr` Placeholder Safeguard (Effective for "v2 new input required" overload)

Trigger condition: When a certain v2 overload because of "new optional input" introduces new input parameter `xo2`, and this parameter is required parameter in v2 (no default value), then additionally generate a same-name safeguard overload.

Signature form: Replace this new input position with `std::nullptr_t`, and `= delete`; optionally overlay `[[deprecated("...")]]` to give more friendly diagnosis.

Example:

```cpp
// Normal overload
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1,
                            const TensorHolder &xo2, int64_t a=0, int64_t b=0);

// Safeguard overload: Prohibit nullptr as placeholder (avoid mistakenly thinking nullptr means "no edge")
[[deprecated("This operator new input xo2 uses TensorHolder fallback disambiguation: if not using xo2 please call old overload Foo(x, xo1[, a]); if xo2 is scalar/vector constant please use EsGraphBuilder::CreateScalar/CreateVector/CreateConst explicit construction.")]]
FORCE_INLINE EsTensorLike Foo(const EsTensorLike &x, const EsTensorLike &xo1,
                            std::nullptr_t /*xo2*/, int64_t a=0, int64_t b=0) = delete;
```

#### 4.3.2 Runnable Dump Rules (Corresponding to Two Solutions)

- A0 (single signature, new input default `= nullptr`):
  - xo2 has no edge: dump can directly omit xo2 (utilize default value)
  - xo2 has edge: dump directly pass xo2
- Default solution (`EsTensorLike` as new input type, v2 xo2 required):
  - xo2 has no edge: dump output v1 form `Foo(x, xo1[, attr...])` (not output `Foo(x, xo1, nullptr, ...)`)
  - xo2 has edge: dump output v2 form `Foo(x, xo1, xo2[, attr...])`
- Fallback solution A (`TensorHolder xo2`):
  - xo2 if is scalar/vector constant: dump must first output `builder.CreateScalar/CreateVector/CreateConst`, then pass product as `xo2`
  - Prohibit dump using `Foo(x, xo1, 0)` type scalar literal form to express `xo2` (this form fixed for attribute expression, like `a=0`)

## 5. ES Integration Points (Minimal)

- Historical prototype library's directory/format/archiving process belongs to protocol content, see [protocol document](history_op_registry_protocol.md).
- ES scenario's main integration point is `generate_es_package` wrapping of `gen_esb` (parameter passing and build flow), and "overload generation and disambiguation processing rules" defined in Chapter 4.

## 6. Special Case Handling

### 6.1 First Enable of Historical Prototype Library

**Scenario**: At some version (like 8.0.RC1) first enable historical prototype library, previous versions not archived

**Processing strategy**:
1. Treat current version as "baseline version"
2. Only generate current version's API, not generate overloads
3. In subsequent versions, use baseline version as starting point to generate overloads

Specifically, 2026/3/30 version planned as first version to enable historical prototype library, but 2025/12/30 is ES graph building's first enable version, therefore before 330 commercial release, need to first generate 2025/12/30 historical prototype data, then generate overload version C++ interfaces, simultaneously archive 330 historical prototype data

### 6.2 Operator Not Existent in Historical Versions

**Scenario**: Current version newly added operator, not existent in historical versions

**Processing strategy**:
- Only generate current version's API
- Not generate overloads (because no historical versions)
- C++ interface and C interface one-to-one correspondence

## 7. Summary

Historical prototype library is key infrastructure in ES graph building system supporting C++ API compatibility:

1. **Core function**: Store historical versions' IR definitions, for `gen_esb` to generate C++ overload interfaces
2. **Key principles**:
   - C interface: Single version, includes all parameters
   - C++ interface: Multiple overload versions, all versions within past one year need to generate
   - Python interface: Single version, uses keyword parameters
   - All interfaces ultimately call same C implementation
3. **Data format**: Uses JSON format, as part of ops run package package publication
4. **Overload generation**: Based on historical version differences, generate overloads for optional input evolution
5. **Disambiguation handling**: Detect and avoid overload disambiguation, ensure generated code compilable
6. **Maintenance process**: Before commercial release use `gen_esb` tool to generate data, package into ops run package
7. **Compatibility**: Default operator prototype changes are all compatible, not perform compatibility check; compatibility guarantee handled by IR semantic runtime compatibility handling mechanism

Through historical prototype library, ES graph building system can ensure usability while providing reliable API/ABI compatibility guarantee.

## 8. Appendix: Existence Matrix and Disambiguation Detection Examples

### A.1 Existence Matrix (Input/Attribute)

One-year window versions are `V1 < V2 < V3`, this operator's input name union across versions is `{x, xo1, xo2}`, attribute name union is `{a, b}`.

Input existence matrix (`1` represents this input definition exists in that version IR):

| version | x | xo1 | xo2 |
|---|---:|---:|---:|
| V1 | 1 | 1 | 0 |
| V2 | 1 | 1 | 1 |
| V3 | 1 | 1 | 1 |

Attribute existence matrix:

| version | a | b |
|---|---:|---:|
| V1 | 1 | 0 |
| V2 | 1 | 0 |
| V3 | 1 | 1 |

From this we can get:
- New addition points: `new_inputs(V2)={xo2}`, `new_attrs(V3)={b}`
- First appearance: `xo2` first appears in `V2`, `b` first appears in `V3`

### A.2 Gate1/Gate2 "Interval Intersection" and Disambiguation Examples

Two candidate overloads:
- f1: `Foo(x, xo1, int64_t a=0)`, callable range `R1=[2,3]`
- f2: `Foo(x, xo1, TensorHolder xo2, int64_t a=0)`, callable range `R2=[3,4]`

Gate1 (intersection): `R1 ∩ R2 = [2,3] ∩ [3,4] = [3,3]`, indicates only when call passes 3 arguments, both can simultaneously become candidates.

Gate2 (argument can simultaneously match):
- Call `Foo(x, xo1, 0)`:
  - For f1: `0` can convert to `int64_t`, can match
  - For f2: `0` cannot convert to `TensorHolder`, cannot match
  - Therefore ultimately not disambiguous, and fixed interpreted as `a=0`

If change f2's `xo2` type to `EsTensorLike` (and exists `EsTensorLike(int64_t)` implicit construction), then `Foo(x, xo1, 0)` can simultaneously match f1 and f2, thus producing disambiguation; this is "conflict time Solution A (TensorHolder fallback)" trigger reason.


### A.3 New Optional Input (EsTensorLike Required + nullptr Safeguard)

When v1/v2 need to simultaneously exist, v2 cannot write new input as `xo2=nullptr` (otherwise `Foo(x, xo1)` will have disambiguation). Recommended form is:
- v1: Express `xo2` has no edge
- v2: `xo2` required, express `xo2` has edge
- Safeguard: `std::nullptr_t` overload prompts user to fallback to v1

Example:
- v1: `Foo(x, xo1, const char* mode="xx")`
- v2: `Foo(x, xo1, EsTensorLike xo2, const char* mode="xx")`
- Safeguard: [deprecated("description information")] `Foo(x, xo1, nullptr, const char* mode="xx") = delete`
