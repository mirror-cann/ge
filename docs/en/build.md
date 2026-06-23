# Source Code Build

## 1. Environment Preparation

Before source code compilation, please complete basic environment setup first. For specific operations, please refer to [Quick Installation](quick_install.md).

## 2. Environment Verification

After installing CANN package, you need to verify whether the environment is normal.

```bash
# View version information provided by CANN Toolkit's version field (default path installation), <arch> represents CPU architecture (aarch64 or x86_64). In WebIDE scenario, please replace /usr/local with /home/developer.
cat /usr/local/Ascend/cann/<arch>-linux/ascend_toolkit_install.info
# View version information provided by CANN ops' version field (default path installation), <opsname> represents the name of the ops sub-package to be queried, please replace according to actual installation path. In WebIDE scenario, please replace /usr/local with /home/developer.
cat /usr/local/Ascend/cann/<arch>-linux/ascend_ops_install.info
```

## 3. Environment Variable Configuration

Choose appropriate commands according to actual scenarios:

  ```bash
  # Default path installation, taking root user as example (for non-root users, replace /usr/local with ${HOME})
  source /usr/local/Ascend/cann/set_env.sh
  # Specified path installation
  source ${install_path}/cann/set_env.sh
  ```

## 4. Source Code Compilation

### 4.1 Download Source Code

Developers can download the source code of this repository through the following commands:

  ```bash
  # Download project source code, taking master branch as example
  git clone https://gitcode.com/cann/ge.git
  ```

### 4.2 Install Dependencies

#### Install Dependencies

  The following lists the dependencies used for GE source code compilation, please note version requirements (if you encounter installation issues, please try switching mirror sources).
  > [!NOTE] Note
  > If using mirror mode for project experience, all dependencies are already included in [init_env.sh](../../scripts/init_env.sh), you can skip this install dependencies step.

- GCC >= 7.3.x

- Python3 >= 3.9.x

  In addition to Python dependencies required by CANN development suite package, you also need to install coverage additionally and add Python3's bin path to PATH environment variable. Command example is as follows:

  ```bash
  pip3 install coverage
  # Modify PYTHON3_HOME below to actual Python installation directory
  export PATH=$PATH:$PYTHON3_HOME/bin
  ```

- CMake >= 3.16.0 (version 3.20.0 recommended)

- bash >= 5.1.16

- ccache/asan/autoconf/automake/libtool/gperf/lcov/libasan/patch/perl/graph-easy (graph-easy is optional)

  ```bash
  # Ubuntu operating system installation command example as follows, other operating system dependencies may have some differences, please install yourself
  # asan taking gcc 7.5.0 version as example, libasan4 is installed, for other versions please install corresponding version asan
  sudo apt-get update
  sudo apt-get install cmake ccache bash lcov libasan4 autoconf automake libtool gperf libgraph-easy-perl patch perl
  ```

- Install other Python dependencies

  ```bash
  ## Enter project source code root directory, execute installation command
  cd ge
  pip3 install -r requirements.txt
  ```

#### Check Compilation Environment

After installation is complete, it's recommended to execute the environment check script to confirm whether the current environment meets compilation requirements.

```bash
bash scripts/check_env.sh
```

Check result explanation is as follows:

| Status | Meaning | Handling Suggestion |
| --- | --- | --- |
| **[PASS]** | Check passed | No action needed |
| **[WARNING]** | Non-critical dependency missing or version deviation | Suggest fixing, does not affect core compilation |
| **[ERROR]** | Critical dependency missing or version incompatible | Must fix, otherwise cannot compile |

> [!NOTE] Note
> All check items and version constraints in the environment check script strictly come from docs/build.md and requirements.txt. If documents and dependencies are updated, please synchronize modification to [script](../../scripts/check_env.sh).

### 4.3 Compilation

> [!NOTE] Note
> If your compilation environment cannot access the network, since source code cannot be downloaded through `git` command, you need to download source code and third-party library dependencies in a networked environment, then manually upload to target environment. Please refer to [Offline Compilation Guide](./user-guides/offline_compile.md).
> If your compilation environment can access the network, after downloading source code through `git` command, open source third-party software will be automatically downloaded during compilation process.

If your compilation environment can access the network, or has completed [Offline Compilation Guide](./user-guides/offline_compile.md), `GE` provides one-click compilation capability. Enter the code repository root directory and compile through the following command:

  ```bash
  bash build.sh --<pkg_type>
  ```

--\<pkg_type> (optional): Represents sub-package type, values include `ge_compiler`, `ge_executor` and `dflow`. Different compilation parameters compile different sub-packages. If not set, all three sub-packages will be compiled simultaneously.

More compilation parameters can be viewed through `bash build.sh -h`. After successful compilation, `cann-<component>_<version>_<arch>.run` software package will be generated in the `build_out` directory.

- `<component>` represents sub-package name, values include ge-compiler, ge-executor and dflow-executor.
- `<version>` represents version number.
- `<arch>` represents operating system architecture, values include x86_64 and aarch64.

### 4.4 Local Verification (UT/ST)

> [!NOTE] Note
> If your compilation environment cannot access the network, please ensure you have completed the [Offline Compilation Guide](./user-guides/offline_compile.md) in [4.3 Compilation](#43-compilation) chapter.

After compilation is complete, users can perform developer testing.

- Compile and execute `UT` test cases:

  ```bash
  # Compile and execute all UT test cases
  bash tests/run_test.sh --ut
  # Compile and execute specific UT test cases (recommended)
  bash tests/run_test.sh --ut=${TARGET}
  ```

  --ut (required): You can specify `${TARGET}` to compile specific object's UT test cases. Available values can be viewed through `bash tests/run_test.sh -h`.

- Compile and execute `ST` test cases:

  ```bash
  # Compile and execute all ST test cases
  bash tests/run_test.sh --st
  # Compile and execute specific ST test cases (recommended)
  bash tests/run_test.sh --st=${TARGET}
  ```

  --st (required): You can specify `${TARGET}` to compile specific object's ST test cases. Available values can be viewed through `bash tests/run_test.sh -h`.

- Collect code coverage:

  Using the `-c` parameter of the `tests/run_test.sh` script can generate code coverage statistics files during test case execution.

  **Prerequisites**:
  - Ensure `lcov` tool is correctly installed
  - `gcc` and `gcov` on the compilation and runtime environment must be matching versions

  **Usage**:

  ```bash
  bash tests/run_test.sh -c [other parameters]
  ```

  **Output Location**: Generated coverage files are located in the `cov/` directory under the code root directory.

- Execute specified test cases:
  
  After UT/ST execution is complete, you can perform individual testing by directly executing test executable files.

  **Prerequisites**:
  - Have executed the corresponding UT/ST test command and correctly generated test executable files.

  **Usage**:
  - Example: Have executed `bash tests/run_test.sh --ut=ge_common`, and want to individually execute and verify `ut_libge_multiparts_utest`:
  
  ```bash
  # UT/ST testing will stub some dependent .so files, need to clear LD_LIBRARY_PATH and ASCEND_OPP_PATH environment variables to avoid interference from CANN/Ascend environment already installed on host machine
  unset LD_LIBRARY_PATH
  unset ASCEND_OPP_PATH
  ./tests/ge/ut/ge/ut_libge_multiparts_utest # You can view included individual cases through --gtest_list_tests, and execute individual cases through --gtest_filter parameter
  ```

- Clean build artifacts:

  `UT/ST` test case compilation output directories are `build_ut` and `build_st`. If you want to clear historical compilation records, you can execute the following operations:

  ```bash
  rm -rf build_ut/ build_st/ output/ build/ build_out/ cov/
  ```

> [!NOTE] Description
> Detailed command parameters supported by `tests/run_test.sh` script can be viewed through `bash tests/run_test.sh -h`.

### 4.5 Installation and Uninstallation

- Installation

  After local verification is complete, you can execute the following command to install the compiled `GE` software package. When executing the installation command, please ensure the installation user has executable permission for the software package.

  ```shell
  ./cann-<component>_<version>_<arch>.run --full --install-path=${install_path}
  ```

  > [!CAUTION] Note
  > * The installation path here (whether default or specified) needs to be consistent with the path where Toolkit package was installed earlier. After installation is complete, the `GE` software package compiled by the user will replace the `GE` related software in the already installed CANN development suite package.
  > * `cann-dflow-executor_<version>_<arch>.run` software package is only needed when using [dflow feature](https://hiascend.com/document/redirect/CannCommunityDataflow). If only using GE graph compilation and execution functions, you can choose not to install. This package contains `cann-udf-compat.tar.gz` (UDF compatibility upgrade package), which will be loaded to Device side during service startup. During loading process, the driver defaults to performing security signature verification to ensure package trustworthiness. The `cann-udf-compat.tar.gz` generated by compiling from source code in this repository does not contain signature header, so you must disable the driver's security signature verification mechanism to use it.
      How to disable signature verification:
      Disabling signature verification functionality depends on Ascend NPU driver software package (Ascend HDK 25.5.T2.B001 and above versions). You can query version and disable signature verification through the npu-smi tool bundled with this Ascend HDK. For details, see [Query Basic Information](https://support.huawei.com/enterprise/zh/doc/EDOC1100540362/4a8adb57?idPath=23710424|251366513|254884019|261408772|252764743), [Set Custom Signature Verification Capability Enable Status](https://support.huawei.com/enterprise/zh/doc/EDOC1100540362/3152813c?idPath=23710424|251366513|254884019|261408772|252764743), [Set Signature Verification Mode](https://support.huawei.com/enterprise/zh/doc/EDOC1100540362/a484ba7b?idPath=23710424|251366513|254884019|261408772|252764743) command documentation, need to execute as root user on physical machine (due to permission issues, WebIDE does not support this yet).
      Taking device 0 as example (where the parameter after -i is device id):
      npu-smi info     # Query basic information, including driver version
      npu-smi set -t custom-op-secverify-enable -i 0 -d 1     # Enable custom signature verification
      npu-smi set -t custom-op-secverify-mode -i 0 -d 0      # Set to "disable signature verification mode"

- Uninstallation

  If you want to uninstall the installed `cann-<component>_<version>_<arch>.run` software package, you can execute the following command.

  ```shell
  ./cann-<component>_<version>_<arch>.run --uninstall --install-path=${install_path}
  ```

  When executing, you need to replace the software package name in the above command with the actual custom `cann-<component>_<version>` software package name.

After installation is complete, you can refer to [Sample Execution](../../examples/README.md) to run samples.