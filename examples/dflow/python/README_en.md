# Python Sample Usage Guide

## Directory Structure

```tree
├── README.md  Sample usage guide
├── config
│   ├── model_generator.py  Script to generate models needed for test cases
│   ├── add_func.json  UDF configuration file for test cases
│   ├── add_graph.json  Computation graph compilation configuration file for test cases
│   ├── data_flow_deploy_info.json  Deployment location configuration file for sample_udf_python
│   ├── multi_model_deploy.json  Deployment location configuration file for sample_multiple_model
│   ├── sample_npu_model_deploy_info.json  Deployment location configuration file for sample_npu_model
│   ├── sample_pytorch_deploy_info.json  Deployment location configuration file for sample_pytorch
│   └── invoke_func.json  UDF call NN compilation configuration file for test cases
├── sample_base.py  This sample demonstrates basic DataFlow API graph construction, including construction and execution of three types of nodes: UDF, GraphPp, and UDF executing NN inference
├── sample_udf_python.py  This sample demonstrates python dataflow calling udf python process
├── sample_exception.py  This sample demonstrates enabling exception reporting
├── sample_pytorch.py  This sample demonstrates DataFlow combined with pytorch for online model inference
├── sample_npu_model.py  This sample demonstrates DataFlow online inference, with udf using npu_model to implement model sinking and data sinking scenarios
├── sample_multiple_model.py  This sample constructs pytorch model as funcPp through decorator, and concatenates it with GraphPp executing onnx/pb models
├── sample_perf.py  This sample is a performance profiling sample
├── udf_py
│   ├── udf_add.py  Implement udf multi-func functionality using python
│   └── udf_control.py  Implement udf functionality using python, used to control which func in udf_add is actually executed
└── udf_py_ws_sample  Complete sample for explaining python udf implementation
    ├── CMakeLists.txt  UDF python complete project cmake file sample
    ├── func_add.json   UDF python complete project configuration file sample
    ├── src_cpp
    │   └── func_add.cpp  UDF python complete project C++ source file sample
    └── src_python
        └── func_add.py  UDF python complete project python source file sample

```

## Environment Preparation

- Refer to [Environment Preparation](../../../docs/zh/quick_install.md) to download and install driver/firmware/CANN software packages;

- Python version requirement: python3.11. The specific version should match the python version used when compiling the dataflow wheel package. If you need to use a different python version, you can refer to [Compilation](../../../docs/zh/build.md#43-编译) to recompile the ge_compiler package and install it;

- The model generation script model_generator.py in the config directory depends on tensorflow-cpu and onnx, which need to be installed via pip3 install tensorflow-cpu and pip3 install onnx;

- [sample_pytorch.py](sample_pytorch.py), [sample_npu_model.py](sample_npu_model.py) samples depend on torch_npu and torchvision packages. torch_npu needs to install corresponding **torch** and **torch_npu** packages according to the actual environment (it is recommended to use version 2.1.0 or above, [Acquisition Method](https://gitcode.com/Ascend/pytorch)). torchvision has a matching relationship with torch, and should be installed using pip3 install torchvision after torch is installed.

## Running Samples

The numa_config.json file configuration below refers to [numa_config Field Description and Samples](https://www.hiascend.com/document/detail/zh/canncommercial/800/developmentguide/graph/dataflowcdevg/dfdevc_23_0031.html)

```bash
# Execute the tensorflow raw model generation script in the config directory:
python3 config/model_generator.py
# After execution, add.pb model and simple_model.onnx model will be generated in the config directory

# Optional
export ASCEND_GLOBAL_LOG_LEVEL=3       #0 debug 1 info 2 warn 3 error. Default is error level if not set
# Required
source {HOME}/Ascend/cann/set_env.sh #{HOME} is the CANN software package installation directory, please replace according to the actual installation path
export RESOURCE_CONFIG_PATH=xxx/xxx/xxx/numa_config.json

python3.11 sample_base.py
python3.11 sample_udf_python.py
python3.11 sample_exception.py
python3.11 sample_pytorch.py
python3.11 sample_npu_model.py
python3.11 sample_multiple_model.py
python3.11 sample_perf.py

# Unset this environment variable to prevent affecting non-dflow test cases
unset RESOURCE_CONFIG_PATH

```
