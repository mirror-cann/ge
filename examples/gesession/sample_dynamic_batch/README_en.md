# Sample Usage Guide

## Function Description

This sample is based on Onnx ResNet-50 network (single input, single Batch) and implements image classification functionality in multi-batch scenarios.

In this sample:

1. First use the script transfer_pic.py provided in the sample to convert 2 *.jpg images to *.bin format, while scaling images from 1024*683 resolution to 224*224.
2. Parse the resnet50 model, then directly perform inference on 2 images (batch value 2), obtain inference results, and process the inference results to output top5 confidence class labels.

## Directory Structure

```tree
├── data
│   ├── dog1_1024_683.jpg               // Test data, need to obtain test images as instructed and place in data directory
│   ├── dog2_1024_683.jpg               // Test data, need to obtain test images as instructed and place in data directory

├── model
│   ├── resnet50_Opset16.onnx           // Onnx ResNet-50 network model file

├── scripts
│   ├── build.sh                        // Sample compilation script
│   ├── run.sh                          // Sample execution script
│   ├── transfer_pic.py                 // Convert *.jpg to *.bin, while scaling images from 1024*683 resolution to 224*224

├── src
│   ├── CMakeLists.txt                  // Compilation configuration script
│   ├── sample_dynamic_batch.cpp        // Main function, image classification implementation file

├── CMakeLists.txt                      // Compilation script, calls CMakeLists file in src directory
```

## Environment Requirements

- Correctly install `toolkit` and `ops` packages through installation guide [Environment Preparation](../../../docs/en/quick_install.md#1-environment-preparation)

- Set environment variables (assuming package is installed in `/usr/local/Ascend/`)

    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

## Implementation Steps

1. Login to development environment as running user.

2. After downloading the code and uploading to the environment, please first enter the "examples/gesession/sample_dynamic_batch" sample directory under the root directory.

    Note that the sample directory in the following text refers to the "examples/gesession/sample_dynamic_batch" directory.

3. Prepare ResNet-50 model.
    1. Obtain the ResNet-50 original model.

        You can obtain the ResNet-50 network model file from the following link and upload it to the "sample directory/model" directory in the development environment as the running user. If the directory does not exist, create it yourself.

        - ResNet-50 network model file (*.onnx): Click [Link](https://github.com/onnx/models/blob/main/Computer_Vision/resnet50_Opset16_timm/resnet50_Opset16.onnx) to download this file.

4. Prepare test images.
    1. Please obtain the input images for this sample from the following links and upload them to the "sample directory/data" directory in the development environment as the running user. If the directory does not exist, create it yourself.

        [https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/models/aclsample/dog1\_1024\_683.jpg](https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/models/aclsample/dog1_1024_683.jpg)

        [https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/models/aclsample/dog2\_1024\_683.jpg](https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/models/aclsample/dog2_1024_683.jpg)

    2. Switch to "sample directory/data" directory and execute transfer_pic.py script to convert *.jpg to *.bin, while scaling images from 1024*683 resolution to 224*224. 2 *.bin files will be generated in "sample directory/data".

        ```python
        python3 ../scripts/transfer_pic.py
        ```

        If the script execution reports "ModuleNotFoundError: No module named 'PIL'", it means the Pillow library is missing. Please use **pip3 install Pillow --user** command to install the Pillow library.

## Build and Verification

1. Login to development environment as running user.

2. Please first enter the "examples/gesession/sample_dynamic_batch" sample directory under the root directory.

    Note that the sample directory in the following text refers to the "examples/gesession/sample_dynamic_batch" directory.

3. Switch to "sample directory/scripts" and compile the program.

    ```bash
    bash build.sh
    ```

4. Run the program

    ```bash
    bash run.sh
    ```

5. Execution results

    After successful execution, key prompt messages on the screen are shown below. The index in the prompt messages indicates the class label, and value indicates the maximum confidence for that classification. These values may vary depending on version and environment, please refer to actual conditions:

        [INFO] SAMPLE start to execute.
        [INFO] Initialize ge success
        [INFO] Set device 0 success
        [INFO] Parse model ../model/resnet50_Opset16.onnx success
        [INFO] Graph add success
        [INFO] Graph compile success
        [INFO] Start to process file:../data/dog1_1024_683.bin
        [INFO] Start to process file:../data/dog2_1024_683.bin
        [INFO] Graph run success
        [INFO] Result of picture 1:
        [INFO] top 1: index[162] value[xxxxxx]
        [INFO] top 2: index[161] value[xxxxxx]
        [INFO] top 3: index[166] value[xxxxxx]
        [INFO] top 4: index[167] value[xxxxxx]
        [INFO] top 5: index[163] value[xxxxxx]
        [INFO] Result of picture 2:
        [INFO] top 1: index[267] value[xxxxxx]
        [INFO] top 2: index[266] value[xxxxxx]
        [INFO] top 3: index[265] value[xxxxxx]
        [INFO] top 4: index[153] value[xxxxxx]
        [INFO] top 5: index[99] value[xxxxxx]
        [INFO] Output data success
        [INFO] SAMPLE PASSED

    **Note:**
    The correspondence between class labels and classes is related to the dataset used when training the model. The model used in this sample was trained on the imagenet dataset. You can check the correspondence between imagenet dataset labels and classes on the internet.
    The correspondence between class labels and classes in the current screen output is:
    "161": \["basset", "basset hound"\],
    "267": \["standard poodle"\].
