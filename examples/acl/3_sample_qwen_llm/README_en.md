# Sample Usage Guide

## Function Description

This sample guides how to implement LLM model loading, execution, and result retrieval based on ONNX format Qwen offline network.

In this sample:

1. Use the atc tool to convert ONNX format Qwen model to om format offline model.
2. Load the offline model om file, provide specified input, execute the model and obtain results.
3. Note that this sample does not cover how to obtain ONNX format offline network. For how to export offline network, refer to: [Qwen Offline Model Export Example](https://gitcode.com/Ascend/ModelZoo-PyTorch/blob/master/ACL_PyTorch/built-in/nlp/Qwen2_for_Pytorch/readme.md).

## Directory Structure

```tree
├── model
│   ├── qwen.om                         // ONNX Qwen network model file, need to obtain the om file converted by atc according to instructions and place it in the model directory

├── scripts
│   ├── build.sh                        // Sample compilation script
│   ├── run.sh                          // Sample execution script

├── src
│   ├── acl.json                        // System initialization configuration file
│   ├── CMakeLists.txt                  // Compilation configuration script
│   ├── sample_qwen_llm.cpp             // Main function, implementation file for LLM inference

├── CMakeLists.txt                      // Compilation script, calls CMakeLists file in src directory
```

## Environment Preparation

- Follow the installation guide [Environment Preparation](../../../docs/zh/quick_install.md) to correctly install `toolkit` and `ops` packages

- Set environment variables (assuming packages are installed in `/usr/local/Ascend/`)

    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

## Implementation Steps

1. Log in to the development environment as the running user.

2. After downloading the code and uploading it to the environment, first navigate to the "examples/acl/3_sample_qwen_llm" sample directory under the root directory.

    Note that the sample directory in the following text refers to the "examples/acl/3_sample_qwen_llm" directory.

3. Prepare the Qwen model.
    1. Obtain Qwen ONNX format model.

        You can obtain the Qwen network model file using the following command. The model file needs to be placed in the "sample directory/model" directory. If the directory does not exist, create it yourself. If wget fails, you can also download directly by entering the following link in your browser and upload to the "sample directory/model" directory.

        ```bash
        cd "sample directory/model"
        wget https://ascend-cann.obs.cn-north-4.myhuaweicloud.com/cann_test/qwen.onnx
        ```

    2. Convert the Qwen original model to an offline model (*.om file) adapted for Ascend AI processors. Note that if the generated om file has an architecture suffix (such as qwen_linux_aarch64.om), please rename the file to qwen.om.

        Switch to "sample directory/model" and execute the model conversion script:

        ```bash
        cd "sample directory/model"
        atc \
        --model=./qwen.onnx \
        --output=./qwen \
        --soc_version=Ascend910B4-1 \
        --framework=5 \
        --precision_mode=must_keep_origin_dtype \
        --op_select_implmode=high_precision \
        --external_weight=0 \
        --output_type=FP32 \
        --input_shape 'input_ids:1,512;past_key_0.key:1,2,512,64;past_key_0.value:1,2,512,64;past_key_1.key:1,2,512,64;past_key_1.value:1,2,512,64;past_key_2.key:1,2,512,64;past_key_2.value:1,2,512,64;past_key_3.key:1,2,512,64;past_key_3.value:1,2,512,64;past_key_4.key:1,2,512,64;past_key_4.value:1,2,512,64;past_key_5.key:1,2,512,64;past_key_5.value:1,2,512,64;past_key_6.key:1,2,512,64;past_key_6.value:1,2,512,64;past_key_7.key:1,2,512,64;past_key_7.value:1,2,512,64;past_key_8.key:1,2,512,64;past_key_8.value:1,2,512,64;past_key_9.key:1,2,512,64;past_key_9.value:1,2,512,64;past_key_10.key:1,2,512,64;past_key_10.value:1,2,512,64;past_key_11.key:1,2,512,64;past_key_11.value:1,2,512,64;past_key_12.key:1,2,512,64;past_key_12.value:1,2,512,64;past_key_13.key:1,2,512,64;past_key_13.value:1,2,512,64;past_key_14.key:1,2,512,64;past_key_14.value:1,2,512,64;past_key_15.key:1,2,512,64;past_key_15.value:1,2,512,64;past_key_16.key:1,2,512,64;past_key_16.value:1,2,512,64;past_key_17.key:1,2,512,64;past_key_17.value:1,2,512,64;past_key_18.key:1,2,512,64;past_key_18.value:1,2,512,64;past_key_19.key:1,2,512,64;past_key_19.value:1,2,512,64;past_key_20.key:1,2,512,64;past_key_20.value:1,2,512,64;past_key_21.key:1,2,512,64;past_key_21.value:1,2,512,64;past_key_22.key:1,2,512,64;past_key_22.value:1,2,512,64;past_key_23.key:1,2,512,64;past_key_23.value:1,2,512,64' \
        --log=error
        ```

        - --model: Original model file path.
        - --input_shape: Specify the input shape of the model.
        - --output: Rename the generated qwen_*.om file to qwen.om and store it in the "sample directory/model" directory. It is recommended to use the default settings in the command, otherwise you need to modify the omModelPath parameter value in sample_qwen_llm.cpp before compiling the code.

            ```text
            ret = sampleQwen.PrepareModel("../model/qwen.om");
            ```

        - --soc_version: Ascend AI processor version. For version acquisition, refer to [Link](https://hiascend.com/document/redirect/CannCommunityAtcSocVersion).
        - --framework: Original framework type. 0: Caffe; 1: air offline model; 3: TensorFlow; 5: ONNX.
        - --precision_mode: Set model precision conversion strategy, must_keep_origin_dtype means force keeping original data type.
        - --op_select_implmode: Specify operator implementation mode, high_precision means prioritize computation precision.
        - --external_weight: Whether to store weights separately from the model, 0 means weights are embedded in the .om file.
        - --output_type: Specify the output tensor data type.

## Build and Verification

1. Log in to the development environment as the running user.

2. First navigate to the "examples/acl/3_sample_qwen_llm" sample directory under the root directory.

    Note that the sample directory in the following text refers to the "examples/acl/3_sample_qwen_llm" directory.

3. Switch to "sample directory/scripts" and compile the program.

    ```bash
    bash build.sh
    ```

4. Run the program

    ```bash
    bash run.sh
    ```

5. Execution Results

    After successful execution, the key prompt information on the screen is shown below:

    ```text
    [INFO] The sample starts to run
    [INFO]  SAMPLE start to execute.
    [INFO]  acl init success
    [INFO]  set device success
    [INFO]  create context success
    [INFO]  create stream success
    [INFO]  load model ../model/qwen.om success.
    [INFO]  Start to Process.
    [INFO]  The first five inputs information:
    [INFO]    Input[0], tensorName=input_ids, size=4096 bytes, dtype=9, format=0, dims=1 512
    [INFO]    Input[1], tensorName=past_key_0.key, size=131072 bytes, dtype=1, format=0, dims=1 2 512 64
    [INFO]    Input[2], tensorName=past_key_0.value, size=131072 bytes, dtype=1, format=0, dims=1 2 512 64
    [INFO]    Input[3], tensorName=past_key_1.key, size=131072 bytes, dtype=1, format=0, dims=1 2 512 64
    [INFO]    Input[4], tensorName=past_key_1.value, size=131072 bytes, dtype=1, format=0, dims=1 2 512 64
    [INFO]  Start to execute model.
    [INFO]  The first five outputs information:
    [INFO]    Output[0], tensorName=/lm_head/MatMul:0:logits, size=311164928 bytes, dtype=0, format=0, dims=1 512 151936
    [INFO]    Output[1], tensorName=/model/self_attn/Concat_5:0:present_0.key, size=524288 bytes, dtype=0, format=0, dims=1 2 1024 64
    [INFO]    Output[2], tensorName=/model/self_attn/Concat_6:0:present_0.value, size=524288 bytes, dtype=0, format=0, dims=1 2 1024 64
    [INFO]    Output[3], tensorName=/model/self_attn_1/Concat_5:0:present_1.key, size=524288 bytes, dtype=0, format=0, dims=1 2 1024 64
    [INFO]    Output[4], tensorName=/model/self_attn_1/Concat_6:0:present_1.value, size=524288 bytes, dtype=0, format=0, dims=1 2 1024 64
    [INFO]  predicted_token_id: 33975
    [INFO]  SAMPLE PASSED.
    ```
