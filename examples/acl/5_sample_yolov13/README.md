# 样例使用指导

## 功能描述

该样例基于 YOLOv13 ONNX 模型实现目标检测功能。

在该样例中：

1. 先使用样例提供的脚本 `preprocess.py`，将图片进行 letterbox 缩放（640x640），并归一化到 0~1，转换为 .bin 格式。
2. 加载离线模型 om 文件，对图片进行推理，得到检测结果（包括目标类别、置信度和 bbox 坐标）。
3. 对检测结果进行 NMS（非极大值抑制）后处理，去除重复检测框。
4. 可选：使用 `draw_boxes.py` 将检测框绘制在原始图片上，生成可视化结果。

## 目录结构

```
├── scripts
│   ├── build.sh                     # 编译脚本
│   ├── run.sh                       # 运行脚本
│   ├── preprocess.py                # 图片预处理：letterbox 缩放 + 归一化，生成 .bin 文件
│   └── draw_boxes.py               # 可视化脚本：将检测结果绘制到原始图片上
├── src
│   ├── acl.json                     # ACL 初始化配置文件
│   ├── CMakeLists.txt               # 编译配置
│   └── sample_yolov13.cpp           # 主程序
├── model                            // 模型存放目录，需手动创建
│   └── yolov13.om                   // ATC 转换后的离线模型文件
├── data                             // 数据存放目录，需手动创建
│   ├── bus.jpg                      // 测试图片
│   ├── bus.bin                      // preprocess.py 预处理生成的 .bin 文件
│   └── bus_out.jpg                  // draw_boxes.py 生成的可视化结果
└── CMakeLists.txt                   // 编译脚本入口
```

## 环境准备

- 通过安装指导正确安装 CANN Toolkit 和 OPP 算子包
- 设置环境变量（以默认安装路径为例，请根据实际安装路径调整）：

    ```
    source /usr/local/Ascend/cann/set_env.sh
    ```

    或者（非标准路径安装）：

    ```
    source /home/developer/Ascend/cann-9.0.0/set_env.sh
    ```

## 实现步骤

1. 以运行用户登录开发环境。

2. 下载代码并上传至环境后，进入样例目录。下文中的"样例目录"均指 `examples/acl/5_sample_yolov13` 目录。

3. 准备 YOLOv13 模型。

    1. 获取 YOLOv13 权重文件。

        从 [YOLOv13 Release 页面](https://github.com/iMoonLab/yolov13/releases/tag/yolov13) 下载 `yolov13n.pt` 权重文件：

        ```
        wget https://github.com/iMoonLab/yolov13/releases/download/yolov13/yolov13n.pt
        ```

        **注意**：如使用 GitHub 下载，请配置稳定的网络代理；若下载不稳定，也可通过浏览器访问上述链接手动下载。

    2. 安装 YOLOv13 所需的依赖。

        YOLOv13 使用了自定义模块（`DSC3k2` 等），标准 `ultralytics` 无法直接加载该模型。需从 YOLOv13 源码仓库安装其 fork 版本的 ultralytics：

        ```
        # 下载 YOLOv13 源码仓库（含 fork 版 ultralytics）
        curl -L -o yolov13.zip "https://codeload.github.com/iMoonLab/yolov13/zip/refs/heads/main"
        unzip yolov13.zip

        # 安装 fork 版 ultralytics（若系统已有标准版需先卸载）
        pip3 uninstall ultralytics -y 2>/dev/null
        pip3 install --no-deps yolov13-main/

        # 安装缺失依赖
        pip3 install huggingface_hub seaborn py-cpuinfo
        ```

    3. 导出 ONNX 模型。

        ```python
        import torch
        from ultralytics import YOLO

        model = YOLO("yolov13n.pt")
        model.export(format="onnx", opset=11, imgsz=640)
        ```

        执行后将生成 `yolov13n.onnx` 文件。

    4. 将 ONNX 模型转换为适配昇腾 AI 处理器的离线模型（\*.om 文件）。

        切换到样例目录，执行如下命令：

        ```
        atc --model=yolov13n.onnx --framework=5 --output=yolov13 \
            --soc_version=Ascend910A \
            --input_shape="images:1,3,640,640" \
            --output_type=FP32
        ```

        将生成的 `yolov13.om` 文件放入 `model/` 目录。

        **`--soc_version` 参数说明**：用于指定模型推理运行时使用的昇腾 AI 处理器型号。不同芯片的 soc_version 取值不同，必须与运行环境一致。常用查询方法：

        - **Atlas A2 训练/推理系列、Atlas 训练/推理系列**：执行 `npu-smi info`，取 **Name** 字段值，前加 `Ascend` 即为 soc_version（如 Name 为 `910B4`，则 soc_version = `Ascend910B4`）
        - **Atlas A3 训练/推理系列**：执行 `npu-smi info -t board -i <id> -c <chip_id>`，取 **Chip Name** 和 **NPU Name**，组合为 `Chip Name_NPU Name`（如 Chip Name 为 `Ascend910B4`，NPU Name 为 `1234`，则 soc_version = `Ascend910B4_1234`）。其中 `id` 为设备 ID（通过 `npu-smi info -l` 查询），`chip_id` 为芯片 ID（通过 `npu-smi info -m` 查询）
        - **Ascend 950 系列**：执行 `npu-smi info -t board -i <id>`，取 **Chip Name** 和 **NPU Name** 组合。其中 `id` 为设备 ID（通过 `npu-smi info -l` 查询）

        详细说明参见 [ATC soc_version 参数文档](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/latest/devaids/atctool/atlasatcparam_16_0036.html)。

4. 准备测试图片。

    1. 从 YOLOv13 官方仓库获取测试图片，放入 `data/` 目录：

        ```
        cd data/
        wget https://raw.githubusercontent.com/iMoonLab/yolov13/main/ultralytics/assets/bus.jpg
        ```

    2. 执行预处理脚本，将 jpg 转换为 bin 格式：

        ```
        python3 ../scripts/preprocess.py
        ```

        如果执行脚本报错 "ModuleNotFoundError: No module named 'cv2'"，请使用以下方式安装 OpenCV 库。

        **注意**：在 aarch64 平台（如 Atlas 推理卡）上，`opencv-python` 可能与 libGLdispatch 存在 TLS 内存分配冲突，导致 `ImportError: cannot allocate memory in static TLS block`。建议安装无 GUI 依赖的 `opencv-python-headless`：

        ```
        pip3 install opencv-python-headless
        ```

        若在 x86_64 平台或已确认无 TLS 冲突，也可使用标准 `opencv-python`。

## 构建验证

1. 编译程序：

    ```
    cd scripts/
    bash build.sh
    ```

2. 运行程序：

    ```
    bash run.sh
    ```

3. 执行结果：

    执行成功后，关键提示信息示例如下（置信度和 bbox 坐标会根据模型版本、环境有所不同）：

    ```
    [INFO] acl init success
    [INFO] set device success
    [INFO] create context success
    [INFO] create stream success
    [INFO] load model ../model/yolov13.om success.
    [INFO] start to process file: ../data/bus.bin
    [INFO] detected 5 objects:
    [INFO]   [0] bus (0.94)  bbox: [92,136,559,435]
    [INFO]   [1] person (0.92)  bbox: [110,236,222,535]
    [INFO]   [2] person (0.87)  bbox: [211,241,284,510]
    [INFO]   [3] person (0.86)  bbox: [475,232,560,519]
    [INFO]   [4] person (0.55)  bbox: [80,330,123,515]
    [INFO] result saved to ../data/bus_result.txt
    [INFO] YOLOv13 SAMPLE PASSED.
    ```

4. （可选）可视化检测结果：

    ```
    cd ../data/
    python3 ../scripts/draw_boxes.py
    ```

    执行后生成 `bus_out.jpg`，图片上绘制了检测框和类别标签。

## 说明

- 类别标签使用 COCO 数据集的 80 类标签，支持 person、car、bus 等常见类别。
- 如需处理更多图片，可在 `sample_yolov13.cpp` 的 `main()` 函数中修改 `inputFiles` 列表。
- 如需调整检测阈值，可在代码中修改 `kConfThresh`（置信度阈值）和 `kIouThresh`（NMS IoU 阈值）。
