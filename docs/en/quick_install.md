# Environment Deployment

## 1. Environment Preparation

This project supports source code compilation. Before source code compilation, you need to ensure that CANN software (Toolkit development suite package and ops package (optional)) has been installed. If running samples, you also need to install NPU driver and firmware.

Please choose the software installation method according to the following description:

| Installation Method | Description | Usage Scenario |
| :------------- | :----------------------------------------------------------- | :----------------------------------------------------------- |
| CANNLab | One-stop development platform, providing online directly runnable Ascend environment, no manual installation required.<br>Currently provides single-machine computing power, **installs the latest commercial release CANN package by default**. | Suitable for developers without Ascend devices. |
| Docker | Docker image is an efficient deployment method, one-click deployment of CANN package and essential dependencies.<br>Currently OS only supports Ubuntu operating system, **installs the latest commercial release CANN package by default**. | Suitable for developers with Ascend devices who need to quickly set up the environment. |
| Manual Installation | Manual installation of CANN package and basic dependencies, high flexibility. | Suitable for developers with Ascend devices who want to experience manual CANN package installation or experience the latest master branch capabilities. |

### Method 1: CANNLab

For developers without Ascend devices, you can directly use CANNLab cloud development environment, which is a "**one-stop development platform**". This platform provides online directly runnable Ascend environment for you, with essential driver firmware, software packages and dependencies already installed, no manual installation required.

> **Note**: The environment installs the latest commercial release CANN package by default. Please note the software compatibility when downloading source code. To experience master version capabilities or develop based on master version, please refer to [**Method 3 - Scenario 1**](#method-3-manual-installation-of-software-packages) to install the latest CANN package dependencies.<br>For more introduction about the development platform, please refer to [CANNLab Guide](https://gitcode.com/org/cann/discussions/54).

1. Enter the open source project and click the "`CANNLab`" button, log in with a certified Huawei Cloud account. If not registered or certified, please follow the page prompts to register and certify.

   <img src="../zh/figures/cloudIDE.png" alt="Cloud Platform"  width="750px" height="90px">

2. Create NPU environment and configure specifications according to page prompts. After starting the cloud development environment, click "`Connect > WebIDE`" to enter the one-stop development platform.

   Currently, open source project resources are located in the `/mnt/workspace/gitCode/${gitCode_id}` directory by default, where ${gitCode_id} represents the developer's personal gitCode account.

   <img src="../zh/figures/webIDE.png" alt="Cloud Platform"  width="1000px" height="150px">

### Method 2: Docker Deployment

For developers who do not depend on Ascend devices, if you want to quickly set up a compilation build environment, you can use Docker image deployment.

> **Note**: The image file is relatively large and takes some time to download. Please wait patiently. For docker command options, you can query through `docker --help`.

1. **Install Drivers and Firmware (Runtime Dependencies)**

   For downloading and installing Ascend drivers and firmware on the host machine, please refer to the "Prepare Software Packages" and "Install NPU Driver and Firmware" chapters in the [CANN Software Installation Guide](https://www.hiascend.com/document/redirect/CannCommunityInstWizard). Drivers and firmware are runtime dependencies. If only compiling operators, you don't need to install them.

2. **Download Image**

   - Step 1: Log in to the host machine as root user. Ensure Docker engine (version 1.11.2 or above) is installed on the host machine.
   - Step 2: Pull the image with CANN software package and development dependencies pre-integrated from [Ascend Image Repository](https://www.hiascend.com/developer/ascendhub/detail/17da20d1c2b6493cb38765adeba85884). The command is as follows, please choose according to actual architecture (taking Atlas A2 series products as example, compilation-only scenarios do not need to pay attention):

     ```bash
     # Example: Pull ARM architecture CANN development image
     docker pull --platform=arm64 swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910-ubuntu22.04-py3.11
     # Example: Pull X86 architecture CANN development image
     docker pull --platform=amd64 swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910-ubuntu22.04-py3.11
     ```

3. **Run Docker**

   Choose different startup methods according to usage scenarios:

   - **Scenario 1: Compilation Build Only (No Need to Run Samples)**

     If you only need code compilation build without accessing NPU devices, use the following simplified command:

     ```bash
     docker run --name cann_container -it -u root --privileged=true -v /home/ge/:/home/ge swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910-ubuntu22.04-py3.11 bash
     ```

   - **Scenario 2: Need to Run Samples (Need to Access NPU Devices)**

     If you need to run samples or tests, the container needs to access the host machine's NPU devices. Taking Atlas A2 series products as example:

     ```bash
     docker run --name cann_container \
       --device /dev/davinci0 \
       --device /dev/davinci_manager \
       --device /dev/devmm_svm \
       --device /dev/hisi_hdc \
       -v /usr/local/dcmi:/usr/local/dcmi \
       -v /usr/local/bin/npu-smi:/usr/local/bin/npu-smi \
       -v /usr/local/Ascend/driver/lib64/:/usr/local/Ascend/driver/lib64/ \
       -v /usr/local/Ascend/driver/version.info:/usr/local/Ascend/driver/version.info \
       -v /etc/ascend_install.info:/etc/ascend_install.info \
       -it -u root --privileged=true \
       swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910-ubuntu22.04-py3.11 bash
     ```

     | Parameter | Description | Notes |
     | :--- | :--- | :--- |
     | `--name cann_container` | Specify a name for the container for easy management. | Optional (value can be customized). |
     | `--device /dev/davinci0` | Map NPU device files into the container. | Required (when running samples). Multiple devices can use this parameter multiple times, such as `/dev/davinci0` `/dev/davinci1`. |
     | `--device /dev/davinci_manager` | Ascend device manager, responsible for device resource management. | Required (when running samples). |
     | `--device /dev/devmm_svm` | Device memory management unit. | Required (when running samples). |
     | `--device /dev/hisi_hdc` | Ascend high-definition codec device. | Required (when running samples). |
     | `-v /usr/local/dcmi:/usr/local/dcmi` | Mount DCMI (Device Communication Management Interface) directory. | Required (when running samples). |
     | `-v /usr/local/bin/npu-smi:...` | Mount NPU monitoring tool for viewing NPU status. | Required (when running samples). |
     | `-v /usr/local/Ascend/driver/...` | Mount NPU driver library and version information. | Required (when running samples). |
     | `-v /etc/ascend_install.info:...` | Mount Ascend software installation information. | Required (when running samples). |
     | `-it` | Combination of `-i` (interactive) and `-t` (allocate pseudo-terminal) parameters. | Required |
     | `-u root` | Enter the container as root (administrator). | Recommended |
     | `--privileged=true` | Enable container highest privilege mode. | Recommended |
     | `swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:...` | Specify the Docker image to run. | Required, please ensure this image name and tag are exactly the same as the image you pulled through `docker pull`. |
     | `bash` | Command to execute immediately after container starts. | Required |

     > **Note**:
     > - Scenario 1 is suitable for GE compilation build only, no NPU device support required
     > - Scenario 2 is suitable for scenarios that need to run samples or perform NPU-related tests, requiring NPU driver and firmware installed on the host machine
     > - If using other chip models (such as 950, Atlas A3 series products), please adjust device names in `--device` parameter accordingly

4. **Initialize Environment**

   After entering the container, execute the following commands to initialize the environment:

   - **Scenario 1 (Compilation Build Only)**:

     ```bash
     curl -fsSL https://raw.gitcode.com/cann/ge/raw/master/scripts/init_env.sh | bash
     ```

   - **Scenario 2 (Need to Run Samples, taking Atlas A2 series products as example)**:

     ```bash
     curl -fsSL https://raw.gitcode.com/cann/ge/raw/master/scripts/init_env.sh | bash -s -- --chip-type 910b
     ```

     > **Note**:
     > - For other chip models, please replace the `--chip-type` parameter with the corresponding model (such as `950`, `A3`)

### Method 3: Manual Installation of Software Packages

- **Scenario 1: Experience master version capabilities or develop based on master version**

    1. **Install Drivers and Firmware (Optional, only required for running [samples](../../examples/README.md))**

        Drivers and firmware are runtime dependencies. If only compiling source code, you don't need to install them. Use `npu-smi info` to check if there is NPU-related information. If not, please refer to [CANN Quick Installation](https://www.hiascend.com/cann/download) to complete driver and firmware installation.

    2. **Install CANN Package**

         Please click [download link](https://ascend.devcloud.huaweicloud.com/artifactory/cann-run-mirror/software/master) to get the latest timestamp version, and download the corresponding package according to product model and environment architecture. Installation commands are as follows, for more guidance please refer to [CANN Software Installation Guide](https://www.hiascend.com/document/redirect/CannCommunityInstWizard).

         - Install CANN Toolkit development suite package.

            ```bash
            # Ensure the installation package has executable permissions
            chmod +x Ascend-cann-toolkit_${cann_version}_linux-${arch}.run
            # Installation command
            ./Ascend-cann-toolkit_${cann_version}_linux-${arch}.run --install --install-path=${install_path}
            ```

            - `${cann_version}`: Represents CANN package version number.
            - `${arch}`: Represents CPU architecture, such as `aarch64`, `x86_64`.
            - `${install_path}`: Represents the specified installation path, needs to be installed in the same path as Toolkit package, root user defaults to `/usr/local/Ascend` directory.

         - Install CANN ops operator package (optional, only required for running [samples](../../examples/README.md)).

            ```bash
            # Ensure the installation package has executable permissions
            chmod +x Ascend-cann-${soc_name}-ops_${cann_version}_linux-${arch}.run
            # Installation command
            ./Ascend-cann-${soc_name}-ops_${cann_version}_linux-${arch}.run --install --install-path=${install_path}
            ```

            Where `${soc_name}` represents NPU model name.

         - Install runtime dependencies (optional, only required for running [samples](../../examples/README.md)).

            ```bash
            pip3 install attrs cython numpy decorator sympy cffi pyyaml pathlib2 psutil protobuf==3.20.0 scipy requests absl-py
            ```

- **Scenario 2: Experience released version capabilities or develop based on released version**

    If you want to experience **officially released CANN package** capabilities, please visit [CANN Official Download Center](https://www.hiascend.com/cann/download), select the corresponding version of CANN software package (only supports CANN 8.5.0 and later versions) for installation.
