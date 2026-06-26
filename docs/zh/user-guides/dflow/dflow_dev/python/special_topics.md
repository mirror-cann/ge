# 专题

## UDF开发流程

目前有两种方式来进行UDF开发：

- 通过注解创建UDF，即通过dataflow.pyflow或者dataflow.method装饰函数进行UDF开发。
- 通过创建UDF工程进行UDF开发。

两种方式的区别如下表：

|比较项|通过注解创建UDF|通过工程创建UDF|
|--|--|--|
|优缺点|优点：使用方式简单直接，通过注解的方式即可将任意代码逻辑封装成UDF。无需额外修改用户代码的原始代码逻辑，改造成本小。支持流式输入和流式输出功能。缺点：部分场景存在使用限制|优点：API调用更加灵活。支持UDF调用NN场景。缺点：使用方式较为繁琐。需要侵入式修改用户源码，使用UDF相关API开发。|
|使用限制|不支持UDF调用NN场景。|无|
|推荐场景|在不使用UDF调用NN时，推荐优先使用该方式创建UDF。|UDF调用NN场景，使用该方式创建UDF。|

### 环境准备

- 进行UDF开发前，需要完成驱动固件及开发套件包Ascend-cann-toolkit的安装，详细操作请参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)。
- 进行Python UDF开发前需要安装Python依赖包：pybind11及jinja2。pybind11用于C++和Python之间对象的转换。jinja2用于快速生成工程模板。
- 配置环境变量CANN软件包安装路径ASCEND\_INSTALL\_PATH，UDF编译时根据该环境变量寻找依赖的头文件和so，如果不设置，默认该环境变量取值为“/usr/local/Ascend”。配置示例如下。

    ```shell
    export ASCEND_INSTALL_PATH=CANN包安装路径
    ```

### 通过注解创建UDF

**开发流程**

通过注解进行UDF开发的流程如下图所示：

![](figures/260109170200356.png)

**创建UDF**

以实现add功能的函数为例，介绍如何通过注解dataflow.pyflow和dataflow.method来创建一个UDF，请参考如下示例：

```python
import dataflow as df

# 通过pyflow注解装饰普通函数，将普通函数构建为一个UDF
@df.pyflow()
def add(a, b):
    return a + b

# 通过pyflow注解装饰类，并使用method装饰类方法，将类方法构建为一个UDF
@df.pyflow
class Add():
    @df.method()
    def add(self, a, b):
        return a + b
```

**构建FlowGraph**

通过注解构造完UDF后，通过fnode方法构造FlowNode并参与FlowGraph图的构造，示例如下：

```python
# 定义输入
data0 = df.FlowData()
data1 = df.FlowData()

#定义FlowNode
flow_node0 = add.fnode()
flow_node1 = Add.fnode()

# 构建连边关系
flow_node0_out = flow_node0(data0, data1)
flow_node1_out = flow_node1.add(flow_node0_out, data1)

# 构建FlowGraph
dag = df.FlowGraph([flow_node1_out])
```

### 通过工程创建UDF

**开发流程**

通过工程进行UDF开发的流程如下图所示：

![](figures/260109170411587.png)

**UDF工程创建**

UDF工程分成如下几类，请根据使用场景进行创建。

- 通过UDF实现用户自定义功能：该场景下，用户不调用第三方依赖库，不调用已经实现的NN功能，完全通过编写UDF相关文件实现自定义功能。
- 通过调用NN实现自定义功能：该场景下，用户需要的功能可以通过已有的NN功能承载，用户通过UDF调用NN功能即可。

通过工具创建工程，详见[UDF Python工程创建工具](appendices.md#udf-python工程创建工具)。

**自定义函数开发**

UDF Python开发过程中进行自定义处理函数的编写。本小节介绍进行UDF的开发过程。该部分代码可以通过模板自动生成。

UDF的实现包括两部分：

- UDF实现文件：用户自定义函数功能的代码实现，文件后缀为.py。按场景分为UDF实现文件（通过UDF实现自定义功能）和通过调用NN实现自定义功能。
- UDF封装及注册文件：用户基于工具生成或按照规范自定义cpp文件。用于获取自定义的Python UDF，将其封装成C++函数，并调用FLOW\_FUNC\_REGISTRAR完成函数的注册。

> [!NOTE]说明
>
>- 单P场景下，支持一次输入对应多次输出，或者一次输入对应一次输出，或者多次输入对应一次输出。
>- 2P场景下，仅支持一次输入对应一次输出。
>- 如下以实现add功能的函数为例，介绍如何进行UDF开发、编译、构图及验证。

**UDF实现文件（通过UDF实现自定义功能）**

用户需要在工程的“workspace/src\_python/xxx.py”文件中进行用户自定义函数的开发。以“func\_add.py”为例进行介绍。

**函数分析：**

该函数实现Add功能，把两个输入转换为numpy数组并进行加法运算，将运算结果进行输出。

**明确输入和输出：**

Add函数有两个输入，一个输出。

**注意事项：**

用户在Python文件中定义的函数名称要和cpp文件中获取Python模块的名称保持一致。

如用户在Python文件中定义初始化函数为init\_flow\_func，在cpp的Init函数中查找Python模块属性时，应查找init\_flow\_func。具体实例可参考如下代码。

**函数实现：**

包括初始化函数和实现函数（实现函数可以有多个）。

用户在编写UDF代码时，通常需要导入dataflow.data\_type、dataflow.flow\_func模块。

flow\_func中开放了UDF编写的常用接口，data\_type封装了常用的数据类型。具体模块内容用户可以导入后自行查看。

```python
import dataflow.flow_func as ff
import dataflow.data_type as dt
# 定义类
class Add():
# 类名称和实际开发UDF功能相关
```

- 初始化函数，执行初始化动作。示例如下。

    ```python
        @ff.init_wrapper() # 该描述是必填项，wrapper中将C++的对象和Python开放的对象进行了转换
        def init_flow_func(self, meta_params):  # 入参有且只能有meta_params，类型为MetaParams
            name = meta_params.get_name()
            ff.logger.info("func name is %s", name)
            input_num= meta_params.get_input_num()
            ff.logger.info("input num %d", input_num)
            device_id = meta_params.get_running_device_id()
            ff.logger.info("device id %d", device_id)
            out_type = meta_params.get_attr_tensor_dtype("out_type")
            if out_type[0] != ff.FLOW_FUNC_SUCCESS:
                 ff.logger.error("get dtype failed")
                 return ff.FLOW_FUNC_FAILED
            self.count = 0
            return ff.FLOW_FUNC_SUCCESS

    ```

- 实现函数。

    ```python
    @ff.proc_wrapper() # 该描述是必填项，wrapper中将C++的对象和Python开放的对象进行了转换
    def Add1(self, run_context, input_flow_msgs): # 入参有且只能有MetaRunContext类型对象及FlowMsg类型对象
        for msg in input_flow_msgs:
            ff.logger.error("msg code %d", msg.get_ret_code())
            if msg.get_ret_code() != ff.FLOW_FUNC_SUCCESS:
                ff.logger.error("invalid input")
                return ff.FLOW_FUNC_FAILED
        # add方法下应该只有两个输入
        if input_flow_msgs.__len__() != 2:
            ff.logger.error("invalid input")
            # 返回flow_func模块中定义的错误码
            return ff.FLOW_FUNC_ERR_PARAM_INVALID
        tensor1 = input_flow_msgs[0].get_tensor()
        tensor2 = input_flow_msgs[1].get_tensor()

        dtype1 = tensor1.get_data_type()
        dtype2 = tensor2.get_data_type()
        # 两个输入的datatype预期应该相同
        if dtype1 != dtype2:
            ff.logger.error("input data type is not same")
            return ff.FLOW_FUNC_FAILED
        ff.logger.info("element size %d", tensor1.get_data_size())
        ff.logger.event("data type is same")
        shape1 = tensor1.get_shape()
        shape2 = tensor2.get_shape()
        if shape1 != shape2:
            ff.logger.error("input data shape is not same")
            return ff.FLOW_FUNC_FAILED
        ff.logger.info("shape is same")
        # 申请输出的msg对象
        out = run_context.alloc_tensor_msg(shape1, dt.DT_INT32)
        data_size = out.get_tensor().get_data_size()
        ff.logger.error("data_size %d", data_size)
        ele_cnt = out.get_tensor().get_element_cnt()
        ff.logger.error("ele_cnt %d", ele_cnt)
        np1 = tensor1.numpy()
        np2 = tensor2.numpy()
        a = out.get_tensor().numpy()
        # 通过获取tensor，进一步获取tensor中的数组数据，进行加法运算
        a[...] = np1 + np2
        ff.logger.event("prepare to set output in add1")
        if run_context.set_output(0, out) != ff.FLOW_FUNC_SUCCESS:
            ff.logger.error("set output failed")
            return ff.FLOW_FUNC_FAILED
        self.count += 1
        return ff.FLOW_FUNC_SUCCESS

    ```

- 如果用户需要定义多个功能，可以在同一个类中定义多个实现函数。

    如下示例为一个类中包括两个实现函数：Add1和Add2。

    ```python
    @ff.proc_wrapper()
    def Add1(self, run_context, input_flow_msgs):
        xxxx

    @ff.proc_wrapper()
    def Add2(self, run_context, input_flow_msgs):
        xxxx
    ```

**UDF实现文件（通过调用NN实现自定义功能）**

用户需要在工程的“workspace/src\_python/xxx.py”文件中进行用户自定义函数的开发。

**函数分析：**

该函数实现Add功能。

**明确输入和输出：**

包含两个输入，一个输出。

**函数实现：**

包括初始化函数和实现函数（实现函数可以有多个）。

用户在编写UDF代码时，通常需要导入dataflow.data\_type、dataflow.flow\_func模块

flow\_func中开放了UDF编写的常用接口，data\_type封装了常用的数据类型。具体模块内容用户可以导入后自行查看。

```python
import dataflow.flow_func as ff
import dataflow.data_type as dt
# 定义类
class Add():
# 类名称和实际开发UDF功能相关
```

- 初始化函数，执行初始化动作。示例如下。

    ```python
        @ff.init_wrapper() # 该描述是必填项，wrapper中将C++的对象和Python开放的对象进行了转换
        def init_flow_func(self, meta_params):  # 入参有且只能有meta_params，类型为MetaParams
            name = meta_params.get_name()
            ff.logger.info("func name is %s", name)
            input_num= meta_params.get_input_num()
            ff.logger.info("input num %d", input_num)
            device_id = meta_params.get_running_device_id()
            ff.logger.info("device id %d", device_id)
            out_type = meta_params.get_attr_tensor_dtype("out_type")
            if out_type[0] != ff.FLOW_FUNC_SUCCESS:
                 ff.logger.error("get dtype failed")
                 return ff.FLOW_FUNC_FAILED
            self.count = 0
            return ff.FLOW_FUNC_SUCCESS

    ```

- Add\(\)：用户自定义的计算处理函数。UDF框架在接收到输入tensor的数据后，会调用此方法。

    示例如下：

    ```python
    @ff.proc_wrapper() # 该描述是必填项，wrapper中将C++的对象和Python开放的对象进行了转换
    def AddNN(self, run_context, input_flow_msgs): # 入参有且只能有MetaRunContext类型对象及FlowMsg类型对象
        ret = run_context.run_flow_model("invoke_graph", input_flow_msgs, 1000)
        if ret[0] != ff.FLOW_FUNC_SUCCESS:
            ff.logger.error("run nn failed")
            return ff.FLOW_FUNC_FAILED
        ff.logger.event("run nn success")
        i = 0
        for out in ret[1]:
            if run_context.set_output(i, out) != ff.FLOW_FUNC_SUCCESS:
                ff.logger.error("set output failed")
                return ff.FLOW_FUNC_FAILED
            i = i + 1
        return ff.FLOW_FUNC_SUCCESS
    ```

- 如果用户需要定义多个功能，可以在同一个类中定义多个实现函数。

    如下示例为一个类中包括两个实现函数：Add1和Add2。

    ```python
    @ff.proc_wrapper()
    def Add1(self, run_context, input_flow_msgs):
        xxxx

    @ff.proc_wrapper()
    def Add2(self, run_context, input_flow_msgs):
        xxxx
    ```

**UDF封装及注册**

Python开发UDF需要准备Python文件以外，还需要准备C++的接口调用文件。建议通过[工具](appendices.md#udf-python工程创建工具)直接生成，也可以自定义，示例如下。

C++的类要继承MetaMultiFunc，C++文件中核心内容包括三部分

**Init函数**

```cpp
    int32_t Init(const std::shared_ptr<MetaParams> &params) override
    {
        FLOW_FUNC_LOG_DEBUG("Init enter");
        PyAcquire();
        ScopeGuard gilGuard([this]() { PyRelease(); });
        int32_t result = FLOW_FUNC_SUCCESS;
        try {
            PyRun_SimpleString("import sys");
            std::string append = std::string("sys.path.append('") + params->GetWorkPath() + "')";
            PyRun_SimpleString(append.c_str());

            constexpr const char *pyModuleName = "func_add"; // 与Python UDF的文件名保持一致，如上述例子中的func_add
            constexpr const char *pyClzName = "Add";
            FLOW_FUNC_LOG_INFO("Load py module name: %s", pyModuleName);
            auto module = py::module_::import(pyModuleName);
            FLOW_FUNC_LOG_INFO("%s.%s import success", pyModuleName, pyClzName);
            pyModule_ = module.attr(pyClzName)();
            if (CheckProcExists() != FLOW_FUNC_SUCCESS) {
                FLOW_FUNC_LOG_ERROR("%s.%s check proc exists failed", pyModuleName, pyClzName);
                return FLOW_FUNC_FAILED;
            }
            if (py::hasattr(pyModule_, "init_flow_func")) { // 与Python UDF中定义的初始化函数名称保持一致，如上述例子中的init_flow_func
                result = pyModule_.attr("init_flow_func")(params).cast<int32_t>();
                if (result != FLOW_FUNC_SUCCESS) {
                    FLOW_FUNC_LOG_ERROR("%s.%s init_flow_func result=%d", pyModuleName, pyClzName, result);
                    return result;
                }
                FLOW_FUNC_LOG_INFO("%s.%s init_flow_func success", pyModuleName, pyClzName);
            } else {
                FLOW_FUNC_LOG_INFO("%s.%s has no init_flow_func method, no need init", pyModuleName, pyClzName);
            }
        } catch (std::exception &e) {
            FLOW_FUNC_LOG_ERROR("init failed: %s", e.what());
            result = FLOW_FUNC_FAILED;
        }
        FLOW_FUNC_LOG_DEBUG("FlowFunc Init end.");
        return result;
    }

```

**proc（）函数：**

```cpp
    int32_t AddProc1(
        const std::shared_ptr<MetaRunContext> &runContext, const std::vector<std::shared_ptr<FlowMsg>> &inputFlowMsgs)
    {
        FLOW_FUNC_LOG_INFO("enter");
        PyAcquire();
        ScopeGuard gilGuard([this]() { PyRelease(); });
        int32_t result = FLOW_FUNC_SUCCESS;
        try {
            result = pyModule_.attr("Add1")(runContext, inputFlowMsgs).cast<int32_t>(); // 此处attr的name与Python UDF定义的处理函数一致，如上述例子中的Add1、Add2或Sub1、AddNN
            if (result != FLOW_FUNC_SUCCESS) {
                FLOW_FUNC_LOG_ERROR(".Add Add return %d", result);
                return result;
            }
            FLOW_FUNC_LOG_INFO("call Add result: %d", result);
        } catch (std::exception &e) {
            FLOW_FUNC_LOG_ERROR("proc failed: %s", e.what());
            result = FLOW_FUNC_FAILED;
        }
        return result;
    }
// 如果用户在同一个UDF Python类中编写了多个处理函数，这里可以通过多个C++函数调用Python不同attr的方式，实现多FUNC
    int32_t AddProc2(
        const std::shared_ptr<MetaRunContext> &runContext, const std::vector<std::shared_ptr<FlowMsg>> &inputFlowMsgs)
```

通过FLOW\_FUNC\_REGISTRAR宏将实现类声明为func name，注册到UDF框架中。

```cpp
FLOW_FUNC_REGISTRAR(Add)
    .RegProcFunc("Add1", &Add::AddProc1);
    .RegProcFunc("Add2", &Add::AddProc2);
```

**UDF工程编译**

在udf\_py\_ws\_sample目录下创建build和release目录：

编译完成后在workspace/release目录下生成编译成功的so。

1. 创建build目录。

    ```shell
    mkdir -p build
    ```

2. 创建release目录。

    ```shell
    mkdir -p release
    ```

3. 进入build目录。

    ```shell
    cd build
    ```

4. 工程编译
    - 非交叉编译场景（开发环境和运行环境的架构一致）下示例如下：编译x86\_64类型的so，指定target lib为libadd.so，编译工具为g++。

        ```shell
        cmake .. -DTOOLCHAIN=g++ -DRELEASE_DIR=../release -DRESOURCE_TYPE=X86 -DUDF_TARGET_LIB=add && make
        ```

        - -DTOOLCHAIN：编译工具名称。比如g++。
        - -DRELEASE\_DIR：编译完成后，目标文件存放路径。
        - -DRESOURCE\_TYPE：编译的资源类型，可以取值：X86或者Aarch或者Ascend。
            - X86：目标so需要部署在X86架构的Host时，配置为X86。
            - Aarch：目标so需要部署在Aarch的Host时，配置为Aarch。
            - Ascend：目标so需要部署在Device时，配置为Ascend。

        - -DUDF\_TARGET\_LIB：编译完成后，目标so的名称。比如，设置为add，实际生成libadd.so。

    - 交叉编译（开发环境是x86\_64，运行环境是aarch64）场景下，编译Ascend类型的so,指定target lib为libadd.so，编译工具为\$\{INSTALL\_DIR\}/toolkit/toolchain/hcc/bin/aarch64-target-linux-gnu-g++。\$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

        示例如下：

        ```shell
        cmake .. -DTOOLCHAIN=${INSTALL_DIR}/toolkit/toolchain/hcc/bin/aarch64-target-linux-gnu-g++ -DRELEASE_DIR=../release -DRESOURCE_TYPE=Ascend -DUDF_TARGET_LIB=add && make
        ```

**构建FlowGraph**

构造完UDF后，通过FlowNode和FuncProcessPoint构建FlowGraph图，示例如下：

```python
# 定义输入
data0 = df.FlowData()
data1 = df.FlowData()

# 定义FuncProcessPoint实现Add功能的FlowNode
pp0 = df.FuncProcessPoint(compile_config_path="./add_func.json")
flow_node0 = df.FlowNode(input_num=2, output_num=1)
flow_node0.add_process_point(pp0)

# 构建连边关系
flow_node0_out = flow_node0(data0, data1)

# 构建FlowGraph
dag = df.FlowGraph([flow_node0_out])
```

## 指定DataFlow节点部署位置

**功能介绍**

支持使用配置文件指定DataFlow图节点部署位置。该功能可以实现多卡多实例。分摊计算量，提升性能。

**使用约束**

- 指定部署时必须指定所有节点的部署位置。
- 根据AddGraph的options参数中的ge.experiment.data\_flow\_deploy\_info\_path指定部署配置文件，文件必须存在并且格式正确，具体格式要求请参见**“配置文件格式”**。
- 部署位置必须与节点实际可部署位置一致，比如UDF如果只支持编译Host发布件，就不能指定部署到Device，同样如果节点只能部署到Device，就不能指定部署到Host。
- 同一个节点不能重复配置。
- 同一个节点不支持同时部署在Host和Device。

**使用方法**

使用示例如下。

```python
flow_node0 = df.FlowNode(input_num=1, output_num=2)
flow_node0_out = flow_node0(data0)

# 构建FlowGraph，此处重点说明option使用，实际构造dataflow graph请参考其他章节
options = {
    "ge.experiment.data_flow_deploy_info_path":"./data_flow_deploy_info.json"
}
dag = df.FlowGraph([out for out in flow_node0_out], options)
```

**配置文件格式**

典型配置文件data\_flow\_deploy\_info.json的格式要求示例如下。

```json
{

   "keep_logic_device_order": false,
   "batch_deploy_info": [
        {
            "flow_node_list": ["flowNode1", "flowNode2"],
            "logic_device_list": "0:0:-1:0"
        },
        {
            "flow_node_list": ["flowNode3"],
            "logic_device_list": "0:0:1:1"

        },
        {
            "flow_node_list": ["flowNode4"],
            "logic_device_list": "0:0:0:0,0:0:1:0",
            "invoke_list":[
            {
               "invoke_name":"invoked_flow_graph_name",
               "deploy_info_file":"./data_flow_invoke_flow_graph_deploy_info.json"
             },
            {
               "invoke_name":"invoked_flow_graph_name1",
               "logic_device_list": "0:0:0:0"
             }]
        },
        {
            "flow_node_list": ["flowNode5", "flowNode6"],
            "logic_device_list": "0:0:2~3:0~1"
        },
        {
            "flow_node_list": ["flowNode7", "flowNode8"],
            "logic_device_list": "0:0:0~1:0,0:0:2:1"
        }
    ]
}
```

**表1**  配置项说明

|配置项|配置说明|
|--|--|
|keep_logic_device_order|是否按照用户配置的device_list顺序部署实例。<br>取值如下：<br>- true：是，在多实例场景下，按照用户配置的device_list，对Device进行排序部署。<br>- false：否，在多实例场景下，按照框架内部实现逻辑对Device进行排序部署。<br>默认值：false。|
|**batch_deploy_info**|-|
|flow_node_list|FlowNode节点名列表，支持一个或者多个，多个时使用半角逗号分割。|
|logic_device_list|DataFlow图节点部署位置。<br>**格式为**：clusterid:serverid:deviceid:numaid（pgid/dieid），各字段含义如下：<br>- clusterid：集群的ID，当前固定为0。<br>- serverid：服务器节点的ID，请配置为numa_config.json中的node_id。<br>- deviceid:设备的逻辑编号，对应的是环境变量RESOURCE_CONFIG_PATH指定的numa_config文件中“node>item_list”字段的item顺序，从0开始。<br>- numaid（pgid/dieid）：单设备多计算单元的编号。<br> **典型场景如下**。<br>FunctionPp的heavy_load=true时，会部署到指定节点对应的Host节点。当部署到多个Device时，按照如下进行配置：<br>- 逐个配置：如"0:0:0:0,0:0:0:1"，表示该node会被多实例部署到"0:0:0:0"和"0:0:0:1"两个设备上。<br>- 按照设备范围配置：如"0:0:0~1:0~1"，其中的"~"表示范围，"~"前面必须小于等于后面，"0:0:0~1:0~1"表示该node会被多实例部署到"0:0:0:0"、"0:0:0:1"、"0:0:1:0"、"0:0:1:1"2个设备的2个计算单元。<br>当FlowNode为调用子FlowGraph的父节点时，则不能配置多实例。|
|invoke_list（可选，不配置的情况下，子图部署节点与父节点部署节点一致）|-|
|invoke_name|FlowNode节点嵌套的InvokedClosure的name，对应为AddInvokedClosure接口的参数：name。|
|deploy_info_file|通过该配置项指定DataFlow子图节点部署位置，当invoke_name对应的InvokedClosure为DataFlow子图，可配置DataFlow子图对应的部署位置文件，例如./data_flow_invoke_flow_graph_deploy_info.json，该文件中包含的字段和格式要求请参考data_flow_deploy_info.json。|
|logic_device_list|通过该配置项指定DataFlow子图或者AscendGraph子图节点部署位置，配置方式同batch_deploy_info中的logic_device_list。<br>对同一个子图节点，logic_device_list和deploy_info_file不可以同时配置，否则会报错。<br>对AscendGraph子图节点，logic_device_list和父节点的logic_device_list实例数需要相同，否则会报错。|

## DataFlow子图编译缓存

**功能介绍**

开启DataFlow编译缓存功能，当修改DataFlow单个节点的子图时，只增量编译修改的子图，其余子图使用缓存，从而降低编译耗时。

**使用约束**

- 该功能支持FlowGraph，不支持AscendGraph。
- 当前不支持带资源类算子的模型。
- 缓存不保证跨版本的兼容性，如果版本升级，需要清理缓存目录重新编译生成缓存。
- DataFlow子图发生变化后，原来的缓存文件不可用，用户需要手动删除缓存目录中的缓存文件，重新编译生成缓存文件。

**使用方法**

1. 缓存配置启用。

    配置示例如下。

    ```python
        # 初始化
        SOC_VERSION = os.getenv('SOC_VERSION', 'AscendXXX')
        options = {
            "ge.exec.deviceId":"0",
            "ge.exec.logicalDeviceClusterDeployMode":"SINGLE",
            "ge.exec.logicalDeviceId":"[0:0]",
           "ge.graph_compiler_cache_dir":"./build_cache_dir"
        }
        df.init(options)

        flow_node0 = df.FlowNode(input_num=1, output_num=2)
        flow_node0_out = flow_node0(data0)

        # 构建FlowGraph，此处重点说明option使用，实际构造dataflow graph请参考其他章节
        options = {
            "ge.graph_key":"test_graph_00"
        }
        dag = df.FlowGraph([out for out in flow_node0_out], options)
    ```

    **表1**  参数解释

|参数名|含义|
|--|--|
|ge.graph_compiler_cache_dir|图编译磁盘缓存目录，和ge.graph_key配合使用，ge.graph_compiler_cache_dir和ge.graph_key同时配置非空时图编译磁盘缓存功能生效。<br>配置的缓存目录需要存在，否则会导致编译失败。<br>图发生变化后，原来的缓存文件不可用，用户需要手动删除缓存目录中的缓存文件，包括模型缓存文件、索引文件和变量格式文件，重新编译生成缓存文件。|
|ge.graph_key|图唯一标识，建议取值只包含大小写字母（A-Z，a-z）、数字（0-9）、下划线（_）、中划线（-）并且长度不超过128。不满足条件时，系统会报错。|

    编译完成后，会在指定的目录下生成缓存文件。具体内容请参考“缓存文件生成规则”。

2.  图发生变化后，如果需要重新编译。请参考如下步骤。

    > [!NOTE]说明
    >图变化包括：修改GraphProcessPoint对应的graph或者config.json、修改UDF的实现文件和修改部署信息。
    >1.  手动删除整图缓存的文件（包括模型缓存文件、索引文件和变量格式文件，具体内容请参考“缓存文件生成规则”。
    >2.  重新编译。
    >    编译完成后，会在指定的目录下生成缓存文件。具体内容请参考“缓存文件生成规则”。

**缓存文件生成规则**

生成文件包括：

- 模型缓存文件
- 索引文件，便于用户通过graph\_key快速找到对应的缓存文件，索引文件内容示例如下：

    ```json
    {
        "cache_file_list":[
            {
                "cache_file_name":"./cache_dir/graph_$key1_20230117202307.om",
                "graph_key":"graph_$key1",
                "var_desc_file_name":"./cache_dir/graph_$key1_20230117202307.rdcpkt"
            },
            {
                "cache_file_name":"./cache_dir/graph_$key1_20230117203007.om",
                "graph_key":"graph_$key1",
                "var_desc_file_name":"./cache_dir/graph_$key1_20230117203007.rdcpkt"
            }
        ]
    }
    ```

- 变量格式文件，仅在图中存在变量时生成。用于框架匹配模型缓存文件，如果graph\_key对应的图内变量格式发生变更，则之前缓存的文件将无法直接恢复使用，该场景下会重新触发编译流程重新生成缓存文件。
- 如果开启了权重外置功能，即options选项中配置了ge.externalWeight参数，并且设置为1，则在**ge.graph\_compiler\_cache\_dir**参数指定路径下还会生成weight目录，用于保存原始网络中的Const/Constant节点的权重信息。
- DataFlow子图缓存文件夹，仅在FlowGraph场景下生成。以graph\_key命名，存放DataFlow子图编译时产生的缓存文件。

文件名生成规则：

- 当ge.graph\_key配置值只包含大小写字母（A-Z，a-z）、数字（0-9）、下划线（\_）、中划线（-）并且长度不超过128时
  - 索引文件命名为：ge.graph\_key + “.idx”。
  - 模型缓存文件命名为: ge.graph\_key + 当前时间 + “.om”。
  - 变量格式文件命名为: ge.graph\_key + 当前时间 + “.rdcpkt”。

- 不满足上面条件时，系统会报错。

## DataFlow离线编译

**功能介绍**

DataFlow离线编译是指在开发环境编译，在运行环境上加载和部署，从而实现编译和建链解耦。

**使用约束**

无

**使用方法**

1. 开启图编译缓存。示例如下**。**

    ```python
        # 初始化
        SOC_VERSION = os.getenv('SOC_VERSION', 'AscendXXX')
        options = {
            "ge.exec.deviceId":"0",
            "ge.exec.logicalDeviceClusterDeployMode":"SINGLE",
            "ge.exec.logicalDeviceId":"[0:0]",
           "ge.graph_compiler_cache_dir":"./build_cache_dir",
            "ge.runFlag":"0"
        }
        df.init(options)

        flow_node0 = df.FlowNode(input_num=1, output_num=2)
        flow_node0_out = flow_node0(data0)

        # 构建FlowGraph，此处重点说明option使用，实际构造dataflow graph请参考其他章节
        options = {
            "ge.graph_key":"test_graph_00"
        }
        dag = df.FlowGraph([out for out in flow_node0_out], options)
    ```

2. 配置开发环境。

    通过环境变量RESOURCE\_CONFIG\_PATH配置目标执行环境的numa\_config.json文件。示例如下：

    ```shell
    export RESOURCE_CONFIG_PATH=numa_config.json //用于设置配置异构资源描述信息文件的存储路径。
    ```

    > [!NOTE]说明
    >**numa\_config.json**请参考[numa\_config.json配置](appendices.md#numa_configjson配置)。ipaddr字段可以使用任意IP。

3. 图生成。
    1. 调用feed\_data接口进行图编译，生成缓存文件。

        示例如下。

        ```python
        graph_options = {"ge.graph_key" : "key"}
        # 构建FlowGraph
        dag = df.FlowGraph([flow_node2_out], graph_options)

        # feed data
        feed_data0 = np.ones([1,2], dtype=np.int32)
        feed_data1 = np.array([[1,2]], dtype=np.int32)

        flow_info = df.FlowInfo()
        flow_info.start_time = 0
        flow_info.end_time = 5

        dag.feed_data({data0:feed_data0, data1:feed_data1}, flow_info) # 异步喂数据
        # feed_data接口涉及图编译和加载，如果设置ge.runFlag为0，则只进行图编译：
        graph_options = {"ge.graph_key" : "key",
                         "ge.runFlag": "0"}
        ```

    2. 离线部署。

        将开发环境中graph\_compiler\_cache\_dir路径下的模型缓存文件、索引文件和变量格式文件拷贝到运行环境的graph\_compiler\_cache\_dir路径。具体文件请参考“缓存文件生成规则”。

        Atlas A2 训练系列产品/Atlas A2 推理系列产品下的约束条件如下：

        执行环境的numa\_config.json需要和编译时候保持一致，ipaddr字段除外。

## 数据对齐

**功能介绍**

**了解数据对齐之前，需要先了解什么是数据对。**

**图1**  数据对示意图
![](figures/数据对示意图.png "数据对示意图")

- 定义Model X为数据最早的来源，输出有两个队列，输出的多组数据\[A1',B1'\]、\[A2',B2'\]、\[A3',B3'\]，每组数据是有关联的。
- 定义Model A和Model B为数据处理节点，多实例部署在多张卡上，Model A负责处理数据流A'，Model B负责处理数据流B'，由于是多实例部署，处理完之后会乱序，无法按照Model X输出的原始顺序输出。
- 定义Model C为数据汇聚节点，从数据流A和数据流B中一组一组地取数据进行处理，必须\[A1, B1\],\[A2, B2\],\[A3, B3\]这样的配对关系才能正常处理。

**数据对齐是指UDF节点要求输入数据是数据对。**

按照示意图的数据流，如果不做数据配对，按照框架默认的接收顺序，Model C收到的数据顺序是\[A3, B2\],\[A2, B1\],\[A1, B3\]。由于数据混乱，无法直接处理的，UDF节点收到\[A3, B2\]之后需要自己先缓存，然后等待收到\[A2, B1\]后，才可以正常处理\[A2, B2\]。收到\[A1, B3\]之后，才能处理\[A1, B1\]和\[A3, B3\]。

**使用约束**

框架数据对齐依赖transId和dataLabel作为判断对齐依据，如果一个数据分发给不同的实例，由于处理时序的差异，数据流到达下个节点顺序不保序，导致无法保证配对。

**使用方法**

```python
dag = df.FlowGraph([flow_node_out])
dag.set_inputs_align_attrs(align_max_cache_num=256, align_timeout=60 * 1000, dropout_when_not_align=True) # 最大缓存256组数据，如果超时时间1分钟，则丢弃超时未对齐数据
```

## 配置动态shape

**功能介绍**

配置动态shape可以给予图编译阶段更多shape range信息，编译器用以编译高效的算子和更好地管理内存。设置maxOutputSize用于配置更精准的输出最大值，图执行器会根据此配置在图计算前预先申请好输出内存，提升性能。

**使用约束**

无

**使用方法**

假如有一个动态shape的resize沉图，具体实现如下，该沉图有两个输入，一个是需要resize的图片数据，一个是resize目标大小信息：

```python
def resize_bilinear(x: torch.Tensor, hw: torch.Tensor):
      return F.interpolate(x, size=[hw[0], hw[1]], mode="bilinear", align_corners=True)
```

对于该动态沉图，具体配置如下所示。

- **ge.inputShape**：动态输入的shape范围。
  - 支持按照name设置："input\_name1:n1,c1,h1,w1;input\_name2:n2,c2,h2,w2"，例如："input\_name1:8\~20,3,5,-1;input\_name2:5,3\~9,10,-1"。指定的节点需要放在双引号中，节点中间使用英文分号分隔。input\_name必须是转换前的网络模型中的节点名称。如果用户知道data节点的name，推荐按照name设置。

  - 支持按照index设置："n1,c1,h1,w1;n2,c2,h2,w2"，例如："8\~20,3,5,-1;5,3\~9,10,-1"。可以不指定节点名，节点按照索引顺序排列，节点中间使用英文分号分隔。按照index设置shape范围时，data节点需要设置属性index，说明是第几个输入，index从0开始。

  - 动态维度有shape范围的用波浪号“\~”表示，固定维度用固定数字表示，不限定范围的用-1表示。

- **ge.outputMaxSize（可选）**：指定图输出的最大内存。
  - 设置该参数时，dataflow框架会利用该参数预先为沉图输出分配好内存，可减少一次数据拷贝，提升执行性能。

        假如最大输出shape是\[1,3,2567,3440\]，dtype是float32（fp32存储需4个字节），那么outputMaxSize计算为1x3x2567x3440x4=105965760。

        如果设置不正确，outputMaxSize小于实际输出时会有报错。

  - 不设置该参数时，在模型执行完成时根据实际情况申请输出内存。

- **inputs\_tensor\_desc**
  - **data\_type**：tensor的数据类型。
  - **shape**：tensor的shape信息，动态的维度请设置成-1。
  - **format**：每个输入的format信息需要根据实际的情况设置，支持NCHW, NHWC, ND三种格式。

```python
{
    "build_options": {
        "ge.inputShape": "1,3,1~1728,1~1728;2",
        "ge.outputMaxSize": "107495424"
    },
    "inputs_tensor_desc": [
      {
        "data_type": "DT_FLOAT",
        "shape": [1, 3, -1, -1],
        "format": "NCHW"
      },
      {
        "data_type": "DT_INT32",
        "shape": [2],
        "format": "ND"
      }
    ]
}
```

## 多实例部署

**功能介绍**

当某个节点需要较多的计算资源时，可以考虑采用部署多份的方式来加速计算。

**使用约束**

多实例部署之后，由于实例间计算时序不一致，可能需要开启数据对齐特性保证数据配对。

**使用方法**

开启多实例请参考[指定DataFlow节点部署位置](#指定dataflow节点部署位置)。

具体到使用场景，可以分以下两种方式：

**表1**  并行方式

|并行方式|使用场景|优势|劣势|
|--|--|--|--|
|请求间多实例并行|存在多请求并发，需要提高吞吐率|实现简单，代码不需要特殊修改|无法降低单请求时延。|
|拆分请求多实例加速|需要降低单请求时延|单请求时延低|代码需要适配修改，多实例节点之前的节点需要拆分数据并指定分发策略，多实例节点需要指定数据汇聚策略，多实例后需要对数据进行合并。|

- 并行方式1：请求间多实例并行：

    **图1**  请求间多实例并行
    ![](figures/请求间多实例并行.png "请求间多实例并行")

    图中是由Model X-\>Model A-\>Model B串起来的DataFlow模型，由于Model A计算耗时长，所以对Model A拆成两个实例加速，数据流（A、B、C、D）经过Model X后，根据transaction ID轮询并分发到Model A的两个实例（数据A和C分流到实例1，数据B和D分流到实例2），之后再由Model B实现汇聚。

- 并行方式2：拆分请求多实例加速：

    这种场景需要增加数据拆分和数据合并的逻辑，通过输出时指定BalanceConfig进行均衡分发，控制输出分发和汇聚（此功能当前不支持py\_flow注解执行方式）。

    **图2**  拆分请求多实例加速
    ![](figures/拆分请求多实例加速.png "拆分请求多实例加速")

原始模型为由Model X-\>Model A-\>Model B串起来的DataFlow模型，由于Model A计算耗时长，为了降低端到端时延，对Model A进行多实例加速。详细步骤如下。

1. 请求数据A经过Model X进入Model数据拆分模块，被拆分为4份数据（A1、A2、A3、A4）。该操作需要用户根据自己的业务逻辑进行实施，拆分后的数据需要自行携带相关数据标识用于后续节点进行识别处理（例如数据增加DataId/DataName等信息）。
2. A1和A3被发送到Model A的实例1，A2和A4被发送给Model A的实例2。
3. 经过处理后的A1、A2、A3、A4被发送到Model数据合并模块，合并为数据A。
4. 数据A发送到Model B。

**数据分发示例如下**。

基于用户拆分好的数据A1、A2、A3、A4，如下给出如何进行数据分发。

```python
# 构图阶段
flow_node.set_balance_scatter()

# UDF输出方式一：批量输出，适用于用户一次性把数据拆分完成的场景。
cfg = ff.BalanceConfig(1, 4, ff.AffinityPolicy.NO_AFFINITY) # 各参数含义为：row_num=1，col_num=4（共4份数据），affinity_policy=ff.AffinityPolicy.NO_AFFINITY（不亲和）

cfg.set_data_pos([(0, 0), (0, 1), (0, 2), (0, 3)]) # 数据A1对应的pos为{0，0}，A2对应的pos为{0，1}，A3对应的pos为{0，2}，A4对应的pos为{0，3}
context.set_multi_outputs(0, [msg0, msg1, msg2, msg3], cfg)
# UDF输出方式二：单个输出，适用于用户将数据一个个拆分出来的，先拆分出来的数据可以先处理。
cfg = ff.BalanceConfig(1, 4, ff.AffinityPolicy.NO_AFFINITY)
# 根据拆分出的msg0进行分发
cfg.set_data_pos([(0, 0)])
context.set_output(0, msg0, cfg)
# 根据拆分出的msg1进行分发
cfg.set_data_pos([(0, 1)])
context.set_output(0, msg1, cfg)
# 根据拆分出的msg2进行分发
cfg.set_data_pos([(0, 2)])
context.set_output(0, msg2, cfg)
# 根据拆分出的msg3进行分发
cfg.set_data_pos([(0, 3)])
context.set_output(0, msg3, cfg)
```

**数据合并示例如下。**

```python
# 构图阶段
flow_node.set_balance_gather()
# UDF输出BalanceConfig设置
cfg = ff.BalanceConfig(1, 4, ff.AffinityPolicy.ROW_AFFINITY)# 各参数含义为：row_num=1，col_num=4（共4份数据），affinity_policy=ff.AffinityPolicy.ROW_AFFINITY（按行亲和）
# 每个实例将行相同的数据分发到相同的后续目标节点（如果数据合并节点是单节点，可以不用按照这种指定BalanceConfig的方式发送），数据合并模块对收到的数据进行合并
cfg.set_data_pos([(0, 0)])
context.set_output(0, msg0, cfg)
```

## 流式输入和输出

**功能介绍**

在DataFlow中，流式输入是指：UDF执行一次可以获取多次输入。比如流式输出是指：UDF执行一次可以输出多次。详细介绍和示例请参见“使用方法”。

在用户使用dataflow.pyflow或者dataflow.method构造UDF时，若需要将多次输入数据组batch时，建议使用流式输入；若需要将输出数据拆分成多份分发时，建议使用流式输出。

流式输入通过参数stream\_input='Queue'表示，此时入参类型为FlowMsgQueue，用户可以自行从队列中取数据；流式输出通过Python中yield生成函数表达，函数输出次数与yield的次数一致。

**使用约束**

流式输入场景下，DataFlow框架不支持数据对齐和异常事务处理。

**使用方法**

**流式输入使用示例：**

```python
import dataflow as df

# 使用stream_input参数表示该函数为流式输入，入参类型为Queue，func执行一次会从输入队列a取出两次数据，从输入队列b取出一次数据
@df.pyflow(stream_input='Queue')
def func(a, b):
    data1 = a.get()
    data2 = a.get()
    data3 = b.get()
    return data1 + data2 + data3

@df.pyflow
class Foo():
    # 使用stream_input参数表示该函数为流式输入，入参类型为Queue，func执行一次会从输入队列a取出两次数据，从输入队列b取出一次数据
    @df.method(stream_input='Queue')
    def func(self, a, b):
        data1 = a.get()
        data2 = a.get()
        data3 = b.get()
        return data1 + data2 + data3
```

**流式输出使用示例：**

```python
import dataflow as df

# 使用yield表示函数为流式输出，该示例中func被调度执行1次，会输出5次结果
@df.pyflow
def func(a):
    for i in range(5):
        yield a + i


@df.pyflow
class Foo():
    # 使用yield表示函数为流式输出，该示例中func被调度执行1次，会输出5次结果
    @df.method()
    def func(self, a):
        for i in range(5):
            yield a + i
```
