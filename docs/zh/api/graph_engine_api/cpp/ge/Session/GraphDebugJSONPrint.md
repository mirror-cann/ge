# GraphDebugJSONPrint

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

维测场景下，图加载后（执行完[CompileGraph](CompileGraph.md)及[LoadGraph](LoadGraph.md)流程），使用本接口将图中运行任务信息以JSON格式导出，包括Stream ID、Task ID、Task Type等信息。然后，通过tracing方式（例如chrome://tracing/）查看任务可视化信息。

如果不调用GraphDebugJSONPrint接口，[LoadGraph](LoadGraph.md)时，设置如下环境变量，也会生成运行任务信息的JSON文件，方法如下：

```c++
export DUMP_GE_GRAPH=1
```

生成的JSON文件目录遵循如下规则，文件名为debug\_graph\_\{graph\_id\}\_\{time\}\_\{pid\}\_\{seq\}：

- 若指定环境变量：NPU\_COLLECT\_PATH，生成的JSON文件在$\{NPU\_COLLECT\_PATH\}/extra-info/目录。
- 若指定环境变量：DUMP\_GRAPH\_PATH，生成的JSON文件在$\{DUMP\_GRAPH\_PATH\}/目录。
- 若没有指定环境变量，生成的JSON文件在当前脚本执行目录。

上述环境变量的详细说明请参见《环境变量参考》。

## 函数原型

```c++
Status GraphDebugJSONPrint(const uint32_t graph_id, uint32_t flags, AscendString &json_result) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要导出的图ID。 |
| flags | 输入 | 预留，当前固定配置为0。 |
| json_result | 输出 | JSON格式字符，图中任务信息。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_RTI_MODEL_NOT_LOADED：图未加载。<br>SUCCESS：信息导出成功。<br>FAILED：信息导出失败。 |

## 约束说明

- 调用该接口前，需要确定好Device上分配的内存大小。
- 调用该接口前，需要完成[CompileGraph](CompileGraph.md)及[LoadGraph](LoadGraph.md)流程。

## 调用示例

```c++
ge::Session session;
uint32_t graph_id = 1; //假设graph已经加载
uint32_t flags = 0;
ge::AscendString json_result;
ge::Status ret = session->GraphDebugJSONPrint(graph_id, flags, json_result);
```
