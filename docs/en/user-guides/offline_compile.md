# GE Offline Compilation Guide

## 1. Complete the following preparation work in a networked environment

### Step 1: Repository Download

In a networked environment, enter [this project homepage](https://gitcode.com/cann/ge), and complete source code download through the `Download ZIP` or `clone` button according to the instructions.

### Step 2: Download Open Source Third-Party Software Dependencies

GE depends on the following third-party open source software during compilation:

| Open Source Software | Version | Download Address |
|---|---|---|
| protobuf | 25.1 | [protobuf-25.1.tar.gz](https://gitcode.com/cann-src-third-party/protobuf/releases/download/v25.1/protobuf-25.1.tar.gz) |
| boost | 1.87.0 | [boost_1_87_0.tar.gz](https://gitcode.com/cann-src-third-party/boost/releases/download/v1.87.0/boost_1_87_0.tar.gz) |
| abseil-cpp | 20230802.1 | [abseil-cpp-20230802.1.tar.gz](https://gitcode.com/cann-src-third-party/abseil-cpp/releases/download/20230802.1/abseil-cpp-20230802.1.tar.gz) |
| c-ares | 1.19.1 | [c-ares-1.19.1.tar.gz](https://gitcode.com/cann-src-third-party/c-ares/releases/download/v1.19.1/c-ares-1.19.1.tar.gz) |
| benchmark | 1.8.3 | [benchmark-1.8.3.tar.gz](https://gitcode.com/cann-src-third-party/benchmark/releases/download/v1.8.3/benchmark-1.8.3.tar.gz) |
| grpc | 1.60.0 | [grpc-1.60.0.tar.gz](https://gitcode.com/cann-src-third-party/grpc/releases/download/v1.60.0/grpc-1.60.0.tar.gz) |
| googletest | 1.14.0 | [googletest-1.14.0.tar.gz](https://gitcode.com/cann-src-third-party/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz) |
| json | 3.11.3 | [json-3.11.3.tar.gz](https://gitcode.com/cann-src-third-party/json/releases/download/v3.11.3/json-3.11.3.tar.gz) |
| openssl | 3.0.9 | [openssl-openssl-3.0.9.tar.gz](https://gitcode.com/cann-src-third-party/openssl/releases/download/openssl-3.0.9/openssl-openssl-3.0.9.tar.gz) |
| re2 | 2024-02-01 | [re2-2024-02-01.tar.gz](https://gitcode.com/cann-src-third-party/re2/releases/download/2024-02-01/re2-2024-02-01.tar.gz) |
| symengine | 0.12.0 | [symengine-0.12.0.tar.gz](https://gitcode.com/cann-src-third-party/symengine/releases/download/v0.12.0/symengine-0.12.0.tar.gz) |
| zlib | 1.2.13 | [zlib-1.2.13.tar.gz](https://gitcode.com/cann-src-third-party/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz) |
| makeself | 2.5.0 | [makeself-release-2.5.0-patch1.tar.gz](https://gitcode.com/cann-src-third-party/makeself/releases/download/release-2.5.0-patch1.0/makeself-release-2.5.0-patch1.tar.gz) |
| mockcpp | 2.7 | [mockcpp-2.7.tar.gz](https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h1/mockcpp-2.7.tar.gz) |
| libseccomp | 2.5.4 | [libseccomp-2.5.4.tar.gz](https://gitcode.com/cann-src-third-party/libseccomp/releases/download/v2.5.4/libseccomp-2.5.4.tar.gz) |
| cann-cmake | master | [cmake-master.tar.gz](https://raw.gitcode.com/cann/cmake/archive/refs/heads/master.tar.gz) |

- Method 1 (Recommended): Use the [one-click third-party software download packaging script](./../../../scripts/download_third_party_source.sh) provided by GE repository, usage as follows:
  ```bash
  # Execute in GE repository root directory in a networked environment
  cd ge
  bash scripts/download_third_party_source.sh
  ```
  After successful execution, `opensource.tar.gz` will be generated in the GE repository root directory

- Method 2: Download third-party open source software packages one by one through the third-party open source software list links


## 2. After completing [networked environment preparation](#1-complete-the-following-preparation-work-in-a-networked-environment), log in to the test environment (offline) to continue compilation preparation

### Scenario 1: Use third-party software downloaded by [one-click third-party software download packaging script](../../../scripts/download_third_party_source.sh)

Connect to the test environment (offline), upload the [repository](#step-1-repository-download) and [third-party software dependencies](#step-2-download-open-source-third-party-software-dependencies) source code obtained from the networked environment to your specified directory. The downloaded files are compressed packages and need to be extracted.

After extraction, the project structure is as follows:
```bash
├── ge                               # GE repository source code extracted directory
│  ├── api
│  ├── base
│  └── ...
├── opensource                       # Third-party software dependencies extracted directory
│  └── abseil-cpp-20230802.1.tar.gz
│  └── benchmark-1.8.3.tar.gz
│  └── ...
```

Enter the repository root directory, you can adjust the directory through the following commands:
  ```bash
  # Move files under opensource directory to GE repository
  cd ge
  mkdir output output/third_party
  cp -r ../opensource/* output/third_party
  rm -rf ../opensource
  ```

After adjustment, the project structure is as follows:
```bash
├── ge                               # GE repository source code extracted directory
│  ├── api
│  ├── base
│  ├── ...
│  └── output                        # Third-party software default directory
│     └── third_party
│         └── abseil-cpp-20230802.1.tar.gz
│         └── benchmark-1.8.3.tar.gz
│         └── ...
```

After completion, return to [Build Verification - Compilation](../build.md#43-compilation) section to continue compilation

### Scenario 2: Manually download third-party software one by one through third-party open source software list links

1. Connect to the test environment (offline), upload the [repository](#step-1-repository-download) and [third-party software dependencies](#step-2-download-open-source-third-party-software-dependencies) source code obtained from the networked environment to your specified directory, where the repository is a compressed package and needs to be extracted.

2. Enter the repository root directory, create `output/third_party` and place the third-party software packages in this directory.
  ```bash
  # Create output/third_party directory
  cd ge
  mkdir output output/third_party
  ```

After adjustment, the project structure is as follows:
```bash
├── ge                               # GE repository source code extracted directory
│  ├── api
│  ├── base
│  ├── ...
│  └── output                        # Third-party software default directory
│     └── third_party
│        ├── abseil-cpp-20230802.1.tar.gz
│        ├── benchmark-1.8.3.tar.gz
│        └── ...
```

After completion, return to [Build Verification - Compilation](../build.md#43-compilation) section to continue compilation
