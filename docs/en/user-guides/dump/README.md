# GE Graph Dump Format Specification

## Overview

GE (Graph Engine) supports exporting computation graphs in multiple formats, facilitating developers to view, debug and analyze graph structures. This document introduces three dump formats: `ge_proto`, `onnx` and `readable`, and their characteristics and usage methods.

---

## Dump Format Overview

| Format | File Naming | Main Characteristics |
|------|---------|--------------------------------------------------------------------------------|
| **ge_proto** | `ge_proto*.txt` | protobuf text format, **best information completeness**, can be converted to JSON format file for convenient user problem localization |
| **onnx** | `ge_onnx*.pbtxt` | Based on ONNX model description structure, supports [Netron](https://netron.app/) and other **visualization** tools. Detailed explanation see [Netron Visualization Instructions](#netron-visualization-instructions) |
| **readable** | `ge_readable*.txt` | Similar to Dynamo fx graph style, **highest text readability**. Detailed format specification please refer to [readable_dump.md](./readable_dump.md) |

---

## Dump Usage Methods

### Automatic Dump via Environment Variables

By setting environment variables, dump files can be automatically generated during graph execution:

```bash
# Set graph dump level
export DUMP_GE_GRAPH=1

# Set dump path
export DUMP_GRAPH_PATH=/path/to/dump/directory

# Set dump format
export DUMP_GRAPH_FORMAT="ge_proto|onnx|readable"
```

**Environment Variable Explanation:**

| Environment Variable | Explanation | Example Value |
|---------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|
| `DUMP_GE_GRAPH` | Control graph dump content granularity:<br>- `1`: Full dump including edge relationships and data information<br>- `2`: Basic version dump excluding weights and other data<br>- `3`: Simplified version dump showing only node relationships | `1`, `2` or `3` |
| `DUMP_GRAPH_PATH` | dump file save path:<br>- Can be configured as absolute path or relative path to script execution directory<br>- Path supports uppercase/lowercase letters, numbers, underscores, hyphens, periods, Chinese characters | `/path/to/dump` |
| `DUMP_GRAPH_FORMAT` | dump format, supports `ge_proto`, `onnx`, `readable`, multiple formats separated by `\|` | `readable` or `ge_proto\|onnx` (default value) |
| `DUMP_GRAPH_LEVEL` | Control number of dump graph compilation stages:<br>- **Numeric configuration**:<br>  - `1`: dump graphs from all stages<br>  - `2`: dump graphs from whitelist stages (default value)<br>  - `3`: dump final generated graph (after GE optimization and compilation)<br>  - `4`: dump earliest generated graph (graph after GE parsing and mapping operators)<br>- **String configuration**: Use `\|` to separate, e.g. `"PreRunBegin\|AfterInfershape"`, means dump graphs whose names contain these strings | `1`, `2`, `3`, `4` or `"PreRunBegin\|AfterInfershape"` |

### Export via Graph API

#### C++

```cpp
#include "ge/graph.h"

// Create graph
ge::Graph graph("my_graph");
// ... Build graph structure ...

// Export to different formats
graph.DumpToFile(ge::Graph::DumpFormat::kTxt, "suffix");        // ge_proto
graph.DumpToFile(ge::Graph::DumpFormat::kOnnx, "suffix");       // onnx
graph.DumpToFile(ge::Graph::DumpFormat::kReadable, "suffix");   // readable
```

#### Python

```python
from ge.graph import Graph, DumpFormat

# Create graph
graph = Graph("my_graph")
# ... Build graph structure ...

# Method 1: Export to file
graph.dump_to_file(format=DumpFormat.kTxt, suffix="suffix")        # ge_proto
graph.dump_to_file(format=DumpFormat.kOnnx, suffix="suffix")       # onnx
graph.dump_to_file(format=DumpFormat.kReadable, suffix="suffix")   # readable

# Method 2: Direct print (only readable format supported)
print(graph)  # Direct print readable format to console to view graph structure
readable_str = str(graph)  # Get readable format string, can be used for saving or further processing
```
> For detailed explanation of `ge.graph`, please refer to [graph module](../../design/modules/ge_python/ge_python.md#graph-module)

---

## Appendix

### Netron Visualization Instructions

When opening `ge_onnx*.pbtxt` file in Netron:

- **Node representation**: Each node in the graph represents an operator
- **Edge relationships**: Edge relationships are represented by solid lines with arrows, arrow direction indicates data flow direction (from source node to target node)
- **Node information viewing**: Click operator node to view operator detailed information, key information includes:

    | Attribute Name | Explanation |
    | -------------------- | ------------------- |
    | `type` | Operator type |
    | `name` | Operator name |
    | `input_desc_dtype:x` | Data type of x-th input |
    | `input_desc_layout:x` | Data format of x-th input |
    | `input_desc_shape:x` | Shape of x-th input |
    | `output_desc_dtype:x` | Data type of x-th output |
    | `output_desc_layout:x` | Data format of x-th output |
    | `output_desc_shape:x` | Shape of x-th output |
