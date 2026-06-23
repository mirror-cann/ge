# Sample Usage Guide

## Function Description

This sample implements image classification functionality based on the Onnx ResNet-50 network (single input, single Batch).

In this sample:

1. Use the provided script transfer_pic.py to convert 2 *.jpg images to *.bin format, while scaling the images from 1024*683 resolution to 224*224.
2. Load the offline model om file, perform synchronous inference on 2 images, obtain inference results, process the inference results, and output the top5 confidence class labels.

Before loading the offline model, convert the Onnx ResNet-50 network model file to an offline model adapted for Ascend AI processors.

## Directory Structure

```tree
├── data
│   ├── dog1_1024_683.jpg               // Test data, need to obtain test images according to instructions and place them in the data directory
│   ├── dog2_1024_683.jpg               // Test data, need to obtain test images according to instructions and place them in the data directory

├── model
│   ├── resnet50.om                     // Onnx ResNet-50 network model file, need to obtain the om file converted by atc according to instructions and place it in the model directory

├── scripts
│   ├── build.sh                        // Sample compilation script
│   ├── run.sh                          // Sample execution script
│   ├── transfer_pic.py                 // Convert *.jpg to *.bin, while scaling images from 1024*683 resolution to 224*224

├── src
│   ├── acl.json                        // System initialization configuration file
│   ├── CMakeLists.txt                  // Compilation configuration script
│   ├── sample_resnet50_imagenet_classification.cpp                        // Main function, implementation file for image classification functionality

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

2. After downloading the code and uploading it to the environment, first navigate to the "examples/acl/1_sample_resnet50_imagenet_classification" sample directory under the root directory.

    Note that the sample directory in the following text refers to the "examples/acl/1_sample_resnet50_imagenet_classification" directory.

3. Prepare the ResNet-50 model.
    1. Obtain the ResNet-50 original model.

        You can obtain the ResNet-50 network model file from the following link, and upload the obtained file to the "sample directory/model" directory in the development environment as the running user. If the directory does not exist, create it yourself.

        - ResNet-50 network model file (*.onnx): Click [Link](https://github.com/onnx/models/blob/main/Computer_Vision/resnet50_Opset16_timm/resnet50_Opset16.onnx) to download this file.

    2. Convert the ResNet-50 original model to an offline model (*.om file) adapted for Ascend AI processors.

        Switch to the sample directory and execute the following command (taking Atlas A2 series products as an example):

        ```bash
        cd "sample directory/model"
        atc --model=resnet50_Opset16.onnx --framework=5 --output=resnet50 --soc_version=Ascend910B1 --input_format=NCHW --output_type=FP32
        ```

        - --model: Original model file path.
        - --framework: Original framework type. 0: Caffe; 1: MindSpore; 3: TensorFlow; 5: ONNX.
        - --soc_version: Ascend AI processor version. For version acquisition, refer to [Link](https://hiascend.com/document/redirect/CannCommunityAtcSocVersion).
        - --output_type: Specify the output data type as float32.
        - --output: The generated resnet50.om file will be stored in the "sample directory/model" directory. It is recommended to use the default settings in the command, otherwise you need to modify the omModelPath parameter value in sample_resnet50_imagenet_classification.cpp before compiling the code.

            ```text
            const char* omModelPath = "../model/resnet50.om";
            ```

4. Prepare test images.
    1. You can obtain the sample input images using the following commands. The input images need to be placed in the "sample directory/data" directory. If the directory does not exist, create it yourself. If wget fails, you can also download directly by entering the following links in your browser and upload to the "sample directory/data" directory.

        ```bash
        cd "sample directory/data"
        wget https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/models/aclsample/dog1_1024_683.jpg
        wget https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/models/aclsample/dog2_1024_683.jpg
        ```

    2. Switch to the "sample directory/data" directory and execute the transfer_pic.py script to convert *.jpg to *.bin, while scaling the images from 1024*683 resolution to 224*224. 2 *.bin files will be generated in the "sample directory/data" directory.

        ```python
        python3 ../scripts/transfer_pic.py
        ```

        If the script reports "ModuleNotFoundError: No module named 'PIL'", it means the Pillow library is missing. Please use the **pip3 install Pillow --user** command to install the Pillow library.

## Build and Verification

1. Log in to the development environment as the running user.

2. First navigate to the "examples/acl/1_sample_resnet50_imagenet_classification" sample directory under the root directory.

    Note that the sample directory in the following text refers to the "examples/acl/1_sample_resnet50_imagenet_classification" directory.

3. Switch to "sample directory/scripts" and compile the program.

    ```bash
    bash build.sh
    ```

4. Run the program

    ```bash
    bash run.sh
    ```

5. Execution Results

    After successful execution, the key prompt information on the screen is shown below. The index in the prompt information represents the class label, and value represents the maximum confidence of that classification. These values may vary depending on version and environment, please refer to the actual situation:

        [INFO] acl init success
        [INFO] open device 0 success
        [INFO] create context success
        [INFO] create stream success
        [INFO] load model ../model/resnet50.om success
        [INFO] start to process file:../data/dog1_1024_683.bin
        [INFO] model execute success
        [INFO] top 1: index[161] value[xxxxxx]
        [INFO] top 2: index[xxx] value[xxxxxx]
        [INFO] top 3: index[xxx] value[xxxxxx]
        [INFO] top 4: index[xxx] value[xxxxxx]
        [INFO] top 5: index[xxx] value[xxxxxx]
        [INFO] output data success
        [INFO] start to process file:../data/dog2_1024_683.bin
        [INFO] model execute success
        [INFO] top 1: index[267] value[xxxxxx]
        [INFO] top 2: index[xxx] value[xxxxxx]
        [INFO] top 3: index[xxx] value[xxxxxx]
        [INFO] top 4: index[xxx] value[xxxxxx]
        [INFO] top 5: index[xxx] value[xxxxxx]
        [INFO] output data success
        [INFO] SAMPLE PASSED

    **Note:**
    The correspondence between class labels and classes is related to the dataset used when training the model. The model used in this sample is trained based on the imagenet dataset. You can check the correspondence between imagenet dataset labels and classes on the internet.
    The correspondence between class labels and classes in the current screen information is as follows:
    "161": ["basset", "basset hound"],
    "267": ["standard poodle"].
