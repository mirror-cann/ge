# Directory Structure

```tree
├── 01_udf_add
│   ├── CMakeLists.txt
│   └── add_flow_func.cpp Single func interface definition of add function
├── 02_udf_call_add_nn
│   ├── CMakeLists.txt
│   └── call_nn_flow_func.cpp UDF calls tensorflow model
├── 03_udf_add_multi_func
│   ├── CMakeLists.txt
│   └── add_flow_func.cpp Multi-Func interface definition of add function
├── 04_control_func
│   ├── CMakeLists.txt
│   └── control_func.cpp Activates one of the multi funcs defined in 03 according to input
└── README.md
```

## Development Guide

01_udf_add and 02_udf_call_add_nn are single func call samples.

1. Include external header file meta_flow_func.h

2. Define processing class, public inheritance from MetaFlowFunc base class, overload Init and Proc functions

   * Init function performs initialization actions, such as variable initialization, attribute acquisition, etc. UDF framework will call this function during initialization phase
   * Proc contains user-defined computation processing logic. UDF framework will call this function after receiving input data

3. UDF registration, implemented through REGISTER_FLOW_FUNC macro to map class declaration to user-defined UDF function name, and register to UDF framework. For example:
   EGISTER_FLOW_FUNC("call_nn", CallNnFlowFunc);

   call_nn: User-defined UDF function name

   CallNnFlowFunc: Actual executing class name, needs to be consistent with class name in cpp file

03_udf_add_multi_func and 04_control_func are multi-func call samples

   1. Include external header file meta_multi_func.h
   2. Define processing class, public inheritance from MetaMultiFunc base class, overload Init and Proc functions

      * Init function performs initialization actions, such as variable initialization, attribute acquisition, etc. UDF framework will call this function during initialization phase

      * Proc contains user-defined computation processing logic. UDF framework will call this function after receiving input data. Multiple proc functions can be defined

   3. UDF registration, implemented through FLOW_FUNC_REGISTRAR macro to map class declaration to user-defined UDF function name, and register to UDF framework. For example:
   FLOW_FUNC_REGISTRAR(AddFlowFunc)
   .RegProcFunc("Proc1", &AddFlowFunc::Proc1)
   .RegProcFunc("Proc2", &AddFlowFunc::Proc2);

## Compilation Guide

After UDF function development is completed, you can use the following compilation instructions to check whether CMakeLists file and cpp source code have any issues.

```bash
source {HOME}/Ascend/cann/set_env.sh #{HOME} is the CANN software package installation directory, please replace according to the actual installation path
# T
ache 01_udf_add as an example
cd 01_udf_add
mkdir build
cd build
# If host is x86, compile x86 type so
cmake -S .. -DTOOLCHAIN=g++ -DRELEASE=../release -DRESOURCE_TYPE=x86 -DUDF_TARGET_LIB=udf
# If host is arm, compile arm type so
cmake -S .. -DTOOLCHAIN=g++ -DRELEASE=../release -DRESOURCE_TYPE=Aarch -DUDF_TARGET_LIB=udf
# If compiling Ascend type so, need to specify g++ in the environment where cann package is installed
cmake -S .. -DTOOLCHAIN=$ASCEND_HOME_PATH/toolkit/toolchain/hcc/bin/aarch64-target-linux-gnu-g++ -DRELEASE_DIR=../release -DRESOURCE_TYPE=Ascend -DUDF_TARGET_LIB=udf
make -j 64
```
