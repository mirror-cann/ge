# 通过FlowOperator构建FlowGraph

## 什么是FlowOperator

FlowOperator是FlowGraph的节点基类，衍生类有FlowData和FlowNode两种类型。

### 什么是FlowData

FlowData是FlowGraph的输入节点。FlowData定义输入节点的名称和index。详细信息请参考FlowData类。

### 什么是FlowNode

FlowNode是FlowGraph的计算节点。FlowNode定义了计算节点的名称、输入个数、输出个数，FlowNode的实际计算逻辑由AddPp方法添加的ProcessPoint决定，ProcessPoint可以是FunctionPp，也可以是GraphPp。详细信息请参考FlowNode类。

- GraphPp

  GraphPp的主要函数原型包括如下。

  ```cpp
  GraphPp(const char *pp_name, const Graphbuilder &builder) // GraphPp构造函数。
  GraphPp &SetCompileConfig(const char *json_file) // 设置GraphPp的json配置文件路径和文件名。用于GraphPp和AscendGraph的映射。
  ```

  以Add功能为例，介绍如何构建一个GraphPp。如下示例中，Add有两个输入data0和data1，GraphPp和AscendGraph的映射是通过GraphPp的构造方法里的BuildGraph参数传递的，add\_graph\_config.json用于配置AscendGraph的编译信息。

  ```cpp
  ge::Graph BuildGraph()
  {
      auto data0 = op::Data("data0").set_attr_index(0);
      auto data1 = op::Data("data1").set_attr_index(1);
      auto add = op::Add("add").set_input_x1(data0).set_input_x2(data1);
      ge::Graph graph("Graph");
      graph.SetInputs({data0, data1}).SetOutputs({add});
      return graph;
  }
  auto graph_pp = dflow::GraphPp("graph_pp", BuildGraph)
                  .SetCompileConfig("add_graph_config.json");
  ```

- FunctionPp

  FunctionPp的主要函数原型包括如下。

  ```cpp
  FunctionPp(const char_t *pp_name) // FunctionPp构造函数。
  FunctionPp &SetCompileConfig(const char *json_file)  // 用于FunctionPp和UDF的映射，json_file里配置了UDF的编译信息。
  FunctionPp &SetInitParam(const char *attr_name, const ge::DataType &value) // 设置FunctionPp的初始化参数。
  ```

  以Add功能为例，介绍如何构建一个FunctionPp。如下示例中，FunctionPp的名称叫"func\_pp"，FunctionPp的功能通过UDF实现，FunctionPp和UDF的映射通过add\_func\_config.json来实现。

  > [!NOTE]说明
  >UDF实现可以使用C++，也可以使用Python。

  ```cpp
  auto function_pp = dflow::FunctionPp("func_pp")
                      .SetCompileConfig("add_func_config.json")
                      .SetInitParam("out_type", ge::DT_UINT32);
  ```

## 如何通过FlowOperator构建FlowGraph

### 功能介绍

用户使用创建好的FlowOperator节点，创建一个FlowGraph实例，并在FlowGraph中设置输入、输出，从而完成Graph构建。

![](figures/260109163332946.png)

### 使用FlowOperator衍生接口定义节点

1. 包含的头文件。

   ```cpp
   #include "flow_graph/data_flow.h"
   ```

2. 创建FlowOperator实例。

   支持如下两种类型

    - FlowData

        ```cpp
        auto data0 = dflow::FlowData("Data0", 0);
        auto data1 = dflow::FlowData("Data1", 1);
        ```

    - FlowNode

        ```cpp
        auto node0 = dflow::FlowNode("node0", 2, 1);
        ```

    注意：图中的data名称和node名称**必须唯一**。

3. 设置FlowOperator输入。

    只有FlowNode类型的FlowOperator才需要设置输入。

    设置FlowOperator输入：通过"SetInput"设置，例如：

    ```cpp
    auto node0 = dflow::FlowNode("node0", 2, 1)         // 创建FlowNode的FlowOperator实例
      .SetInput(0, data0)                               // 设置FlowNode第一个输入为data0
      .SetInput(1, data1);                              // 设置FlowNode第二个输入为data1

### FlowOperator连接边表达

FlowOperator之间的连边为数据边。数据边用于指定FlowOperator的输入。数据边表达如下。

![](figures/260109163658562.png)

上图的示例代码和注释如下。

```cpp
auto data0 = dflow::FlowData("Data0", 0);
auto data1 = dflow::FlowData("Data1", 1);
auto node0 = dflow::FlowNode("flow_node0", 2, 2).SetInput(0, data0).SetInput(1, data1); // FlowData只有一个输出，因此在作为FlowNode的输入时，SetInput时不需要传FlowData的输出index。
auto node1 = dflow::FlowNode("flow_node1", 1, 1).SetInput(0, flow_node0, 0); // node0有两个输出，第一个输出作为node1的第一输入，SetInput时需要传node0的输出index=0。
auto node2 = dflow::FlowNode("flow_node2", 1, 1).SetInput(0, flow_node0, 1); // node0有两个输出，第二个输出作为node2的第一个输入，SetInput时需要传node0的输出index=1。
```

### 创建FlowGraph实例

完成FlowOperator后，需要创建FlowGraph实例，并在FlowGraph中设置输入FlowOperator、输出FlowOperator，主要过程为：

1. 包含所需的头文件。

    ```cpp
    #include "flow_graph/data_flow.h"
    ```

2. 创建FlowGraph对象。

    ```cpp
    dflow::FlowGraph flow_graph("flow_graph");
    ```

    相关接口请参考FlowGraph类。
    
3. 设置FlowGraph输入和输出节点，使用到的主要接口为：

    - 设置FlowGraph内的输入节点：SetInputs。
    - 设置FlowGraph内的输出节点：SetOutputs。
    - 如果作为输出节点的FlowNode存在多个输出，也可以指定该FlowNode的某个输出为模型输出SetOutputs（index）。

    如下例子是将整个node2的所有输出作为模型输出场景，如果需要设置部分节点部分输出为模型输出请参考SetOutputs（index）：

    ```cpp
    std::vector<FlowOperator> inputs{data0, data1};
    std::vector<FlowOperator> outputs{node2};
    flow_graph.SetInputs(inputs).SetOutputs(outputs);
    ```

    如果输入为多个FlowData节点，需要保证inputs入参顺序和FlowData节点index属性指定的顺序保持一致，否则后面生成模型时会报错。例如：

    ```cpp
    // 准备第一个输入数据
    auto data0 = dflow::FlowData("Data0", 0);   // 创建data0节点，index属性为0
    // 准备第二个输入数据
    auto data1 = dflow::FlowData("Data1", 1);   // 创建data1节点，index属性为1
    // 设置FlowGraph输入节点
    std::vector<FlowOperator> inputs{data0, data1};
    ```
