# 样例使用指导

## 1、功能描述
本样例使用操作符重载进行构图，旨在帮助构图开发者快速理解操作符重载的定义。
与 `operator_overload` 不同，本目录在执行阶段使用 `Session.run_graph_with_stream_async` 接口。
关于异步执行更多信息，请参考 [异步执行](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/850/graph/graphdevg/atlasag_25_0035.html)。

## 2、目录结构
```angular2html
python/
├── src/
|   ├── common.py                            // 公共逻辑：常量、图构建、输入张量、GE 生命周期
|   ├── make_add_graph.py                    // 异步执行样例
|   └── make_add_graph_custom_allocator.py   // 注册自定义 SamplePoolAllocator 异步执行样例
├── README.md               // README文件
├── run_sample.sh           // 执行脚本
```

## 3、使用方法
### 3.1、准备cann包
- 本样例需要同时安装两套 CANN：最新开发包用于图编译，官网正式发布包提供 [`pyACL`](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/appdevg/acldevg/aclpythondevg_0096.html) 模块用于图执行。本文中的“编译”和“执行”均特指图编译和图执行，不指 GE 工程源码编译。
- 安装请参考：
  - 最新开发包，用于图编译，提供本样例依赖的最新 GE/Python 能力。安装请参考 [环境准备](../../../../docs/zh/quick_install.md) 中“方式三：手动安装软件包 > 场景1：体验master版本能力或基于master版本进行开发”，安装最新版本的 `toolkit` 和 `ops` 包
  - 官网正式发布的 CANN `toolkit` 和 `ops` 包，用于图执行，提供 `pyACL`。安装请参考 [环境准备](../../../../docs/zh/quick_install.md) 中“方式三：手动安装软件包 > 场景2：体验已发布版本能力或基于已发布版本进行开发”，安装官网正式发布版本的软件包
- 设置环境变量 (假设最新开发包安装在/usr/local/Ascend/，官网正式发布包安装在/usr/local/Ascend-release/)
```
source /usr/local/Ascend/cann/set_env.sh
export PYTHONPATH="$PYTHONPATH:/usr/local/Ascend-release/cann/python/site-packages"
```
### 3.2、执行
本目录提供两个可选 target：

#### 3.2.1、默认 allocator 样例
```bash
bash run_sample.sh -t sample_and_run_python
```
该命令会：
1. 生成 dump 图并使用 GE 内置 allocator 异步执行该图

执行成功后会看到：
```
[Success] sample 执行成功，pbtxt dump 已生成在当前目录。该文件以 ge_onnx_ 开头，可以在 netron 中打开显示
```

#### 3.2.2、自定义 allocator 样例
```bash
bash run_sample.sh -t sample_and_run_python_custom_allocator
```
该命令在默认样例基础上，通过 `Session.register_external_allocator` 向 GE 注册自定义 `SamplePoolAllocator`。该类继承 `ge.allocator.Allocator` 基类：按**精确字节数**维护空闲块列表，命中则复用，否则 `acl.rt.malloc` 申请；`free` 时将块放回池内（不立即 `acl.rt.free`）。对象被回收时在 `__del__` 中统一 `acl.rt.free` 缓存块。

执行成功后，终端会出现以下格式的打印（具体行数取决于 GE 对输出 Tensor 的分配/释放；地址与 `size` 以实际运行为准）：
```
[Info] SamplePoolAllocator 已注册到 stream
[SamplePool] new   : addr=0x<设备内存地址>  size=<字节数> B
[Info] 异步执行 Graph 成功！
[SamplePool] cache : addr=0x<设备内存地址>  size=<字节数> B
[Info] SamplePoolAllocator 已注销
[SamplePool] destroy: freed <N> cached blocks
[Info] 运行环境已清理
[Success] sample 执行成功，pbtxt dump 已生成在当前目录。该文件以 ge_onnx_ 开头，可以在 netron 中打开显示
```
#### 输出文件说明

执行成功后会在当前目录生成以下文件：
- `ge_onnx_*.pbtxt` - 图结构的protobuf文本格式，可用netron查看

### 3.3、日志打印
可执行程序执行过程中如果需要日志打印来辅助定位，可以在bash run_sample.sh -t sample_and_run_python之前设置如下环境变量来让日志打印到屏幕
```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1 #日志打印到屏幕
export ASCEND_GLOBAL_LOG_LEVEL=0 #日志级别为debug级别
```
### 3.4、图编译流程中DUMP图
可执行程序执行过程中，如果需要DUMP图来辅助定位图编译流程，可以在 bash run_sample.sh -t sample_and_run_python 之前设置如下环境变量来DUMP图到执行路径下
```bash
export DUMP_GE_GRAPH=2
```

## 4、核心概念介绍

### 4.1、构图步骤如下：
- 创建图构建器(用于提供构图所需的上下文、工作空间及构建相关方法)
- 添加起始节点(起始节点指无输入依赖的节点，通常包括图的输入(如 Data 节点)和权重常量(如 Const 节点))
- 添加中间节点(中间节点为具有输入依赖的计算节点，通常由用户构图逻辑生成，并通过已有节点作为输入连接)
- 设置图输出(明确图的输出节点，作为计算结果的终点)

### 4.2、操作符重载
**概念说明：**
操作符重载是 ES API 提供的语法糖，针对AI算子做语法封装，使构图代码更加简洁和直观

**构图 API 特点：**
- 操作符重载保持与函数调用相同的类型检查和约束
