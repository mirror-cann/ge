# C++ Sample Usage Guide

## Directory Structure

```tree
├── CMakeLists.txt  cmake configuration file
├── README.md  Sample usage guide
├── config
│   ├── model_generator.py  Script to generate models required for test cases
│   ├── add_func_multi.json  Configuration file for multi-func used in test cases
│   ├── add_func_multi_control.json  Configuration file for multi-func used in test cases
│   ├── add_func.json  Configuration file for add FunctionPp used in test cases
│   ├── add_graph.json  Configuration file for add GraphPp used in test cases
│   ├── data_flow_deploy_info.json  Configuration file to specify node deployment location in test cases
│   └── invoke_func.json  Configuration file for udf calling nn used in test cases
├── node_builder.h  Common methods to construct FunctionPp and GraphPp
├── sample_base.cpp  This sample demonstrates basic DataFlow API graph building, including construction and execution of UDF, GraphPp, and UDF executing NN inference types of nodes
├── sample_timebatch.cpp  This sample demonstrates TimeBatch usage method
├── sample_countbatch.cpp  This sample demonstrates CountBatch usage method
├── sample_tensorflow.cpp  This sample demonstrates TensorFlow graph construction DataFlow node method
├── sample_multifunc.cpp  This sample demonstrates multi-func calling method
├── sample_exception.cpp  This sample demonstrates enabling exception reporting method
└── sample_perf.cpp  This sample tests Feed and Fetch interface performance
```

## Environment Requirements

- Reference [Environment Preparation](../../../docs/zh/quick_install.md) to download and install driver/firmware/CANN software packages;
- Model generation script model_generator.py in config directory depends on tensorflow, need to install through pip3 install tensorflow.

## Program Compilation

```bash
# Execute tensorflow original model generation script in config directory:
python3 config/model_generator.py
# After execution completes, generate add.pb model in config directory

source {HOME}/Ascend/cann/set_env.sh  # "{HOME}/Ascend" is CANN software package installation directory, replace according to actual installation path.
mkdir build
cd build
cmake .. && make -j 64
cd ../output
```

## Run Samples

numa_config.json file configuration reference in the following text: [numa_config field description and sample](https://www.hiascend.com/document/detail/zh/canncommercial/800/developmentguide/graph/dataflowcdevg/dfdevc_23_0031.html)

```bash
# Optional
export ASCEND_GLOBAL_LOG_LEVEL=3       #0 debug 1 info 2 warn 3 error Default error level if not set
# Required
source {HOME}/Ascend/cann/set_env.sh # "{HOME}/Ascend" is CANN software package installation directory, replace according to actual installation path.
export RESOURCE_CONFIG_PATH=xxx/xxx/xxx/numa_config.json

./sample_base
./sample_timebatch
./sample_countbatch
./sample_tensorflow
./sample_multifunc
./sample_exception
./sample_perf

# unset this environment variable to prevent affecting non-dflow test cases
unset RESOURCE_CONFIG_PATH

```
