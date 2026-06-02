---
name: ge-ut-st
description: 编译和运行 GE (Graph Engine) 项目的单元测试(UT)和系统测试(ST)。当用户需要编译或运行 GE 项目的 UT/ST 用例时使用此 skill。适用场景包括编译特定的测试目标、运行特定的测试用例、处理测试相关的依赖和环境配置，用户的指令可能是编译并运行测试，运行用例，执行用例，测试用例等，优先使用该skill
---

# GE 单元测试(UT)和系统测试(ST)测试执行

## 工作流程

### 步骤 1: 制定计划
- 首先进入plan mode制定计划

### 步骤 2: 配置 toolkit 环境变量

#### 2.1 检查已保存的路径配置

配置文件路径：`~/.config/ge-ut-st/config.json`

首先检查配置文件是否存在并读取已保存的路径：
```bash
cat ~/.config/ge-ut-st/config.json 2>/dev/null
```

**如果存在已保存的路径**，直接使用该路径，跳过路径选择流程。

**如果不存在已保存的路径**，进入路径选择流程，不要自作主张选择默认路径

#### 2.2 路径选择流程

询问用户选择 toolkit 包安装路径：

**问题：您的 cann-toolkit 包安装路径？**
- **选项：默认路径** — `${HOME}/Ascend`
- **选项：自定义路径** — 输入完整路径

根据选择拼接完整路径：
- 默认路径：`ASCEND_INSTALL_PATH=${HOME}/Ascend/ascend-toolkit/latest/`
- 自定义路径：`ASCEND_INSTALL_PATH=<用户输入>/ascend-toolkit/latest/`

验证路径是否存在：
```bash
ls ${ASCEND_INSTALL_PATH}
```
- 如果目录不存在，让用户确认路径是否正确或重新输入

#### 2.3 询问是否记住选择

路径确认后，询问用户：
> 是否记住此路径配置？
> - **选项：仅本次使用** — 不保存
> - **选项：记住我的选择** — 保存到配置文件

如果用户选择"记住我的选择"，保存配置：
```bash
mkdir -p ~/.config/ge-ut-st
cat > ~/.config/ge-ut-st/config.json << 'EOF'
{
  "cann_toolkit_base_path": "<用户输入的基础路径，例如 /home/ge/Ascend>",
  "cann_toolkit_path": "<完整路径，例如 /home/ge/Ascend/ascend-toolkit/latest/>"
}
EOF
```

#### 2.4 设置环境变量

最后执行：
```bash
source ${ASCEND_INSTALL_PATH}/bin/setenv.bash
```

### 步骤 3: 执行 CMake

**触发条件：**
- 首次构建
- 新增文件
- 修改了 CMake 相关文件

```bash
BASEPATH=$PWD
ASCEND_INSTALL_PATH=<从步骤2获取的路径>
cmake -DCMAKE_BUILD_TYPE=GCOV \
      -DENABLE_OPEN_SRC=True \
      -DENABLE_GE_UT=ON \
      -DENABLE_GE_ST=ON \
      -DENABLE_TEST=True \
      -DENABLE_PKG=ON \
      -DASCEND_INSTALL_PATH=${ASCEND_INSTALL_PATH} \
      -DASCEND_3RD_LIB_PATH=${ASCEND_3RD_LIB_PATH} \
      -S ${BASEPATH} \
      -B ${BASEPATH}/cmake-build-gcov
```

### 步骤 4: 编译受影响的 target

根据修改的或新增的 `tests/` 目录下的 `.cc` 文件，识别影响到哪个 target。

**重要规则：**
- 如果有 2 个及以上的 target：每个 target 使用**子 agent** 处理，避免占用上下文窗口
- **target 编译必须串行，禁止多个 target 并发编译**（多个 target 可以并发执行，因为执行时只占用一个线程）
- 设置 `Bash` 命令超时时间为50分钟
- 输出较多
- core_num=$(($(nproc) - 1))
- 使用 `make --target -j $core_num` 单独编译

### 步骤 5: 执行测试
- `unset LD_LIBRARY_PATH; unset ASCEND_OPP_PATH`，执行用例前需要执行这两条unset命令，原因是用例执行需要首先在同目录下寻找依赖的so
- 使用 `--gtest_filter` 单独运行指定的测试用例。
- 输出较多，注意不要直接读取输出

## 变量说明

- `BASEPATH`: 项目根目录（.claude 所在路径）
- `ASCEND_INSTALL_PATH`: toolkit 安装路径
- `ASCEND_3RD_LIB_PATH`: 第三方依赖库路径

### 全量运行单元测试 (UT)
- 设置 `Bash` 命令超时时间为70分钟
```bash
# 运行所有单元测试
bash tests/run_test.sh --ut

# 运行特定单元测试类别
bash tests/run_test.sh --ut=ge           # 所有 GE 单元测试
bash tests/run_test.sh --ut=ge_common    # GE 通用测试
bash tests/run_test.sh --ut=rt           # 运行时测试
bash tests/run_test.sh --ut=python       # Python 绑定测试
bash tests/run_test.sh --ut=parser       # 解析器测试
bash tests/run_test.sh --ut=dflow        # Dflow 测试
bash tests/run_test.sh --ut=engines      # 所有引擎测试
bash tests/run_test.sh --ut=fe           # 融合引擎测试
bash tests/run_test.sh --ut=aicpu        # AICPU 引擎测试
```

### 全量运行系统测试 (ST)
- 设置 `Bash` 命令超时时间为70分钟
```bash
# 运行所有系统测试
bash tests/run_test.sh --st

# 运行特定系统测试类别
bash tests/run_test.sh --st=ge           # 所有 GE 系统测试
bash tests/run_test.sh --st=engines      # 所有引擎系统测试
```

### 测试选项
```bash
bash tests/run_test.sh -c                # 启用代码覆盖率（需要 lcov）
bash tests/run_test.sh -j16              # 16个并行线程
bash tests/run_test.sh --verbose         # 详细输出
```

## 常用测试命令参考

```bash
# 运行特定 UT
bash tests/run_test.sh --ut=ge_common

# 运行特定 ST
bash tests/run_test.sh --st=ge

# 使用 gtest filter
./test_executable --gtest_filter=TestSuite.TestCase
```

## 注意
- 一般ut和st文件比较大，在读取前先使用wc -l查看文件行，如果太大了不要全读
- 新增加用例需要注意文件结尾的namespace结尾的}，增加用例需要放到}的上方
- 任何cmake, make, gcc命令，或者"输出较多"的任务，都不能直接读取输出，否则会导致token和上下文空间消耗过多，需要先输出到临时文件，然后过滤查看是否有错误。成功后需要清理临时我文件
- ASCEND_INSTALL_PATH 目录下的是cann toolkit包中的安装文件，不能修改
- graph_metadef 目录下定义的是基础数据结构，除非是需求中新增加类型定义，否则不要修改
- 第三方库路径下的文件也不能修改
- 更新toolkit包之后，需要清理缓存重新camke和make
- 由于从头编译时间太长，所以除非SKILL中特意说明，否则尽量不要清理缓存重新编译

## 常见问题

### 常见编译错误
- 如果是rt或者aclrt开头的接口编译报错，需要主动使用skill更新cann toolkit包，如果更新后还未解决，就不要再次尝试了，报告错误停止进一步重试。
比如rtGetRtCapability
```
In function ‘rtError_t rtGetRtCapability(rtFeatureType_t, int32_t, int64_t*)’:
runtime_stub.cc:1492:23: error: ‘FEATURE_TYPE_PERSISTENT_STREAM_UNLIMITED_DEPTH’ was not declared in this scope
```
- 编译代码如果报缺少下面的软件，需要执行`sudo apt-get install cmake ccache bash lcov libasan4 autoconf automake libtool gperf libgraph-easy-perl patch`安装
- 如果缺少python相关包，执行` pip3 install -r requirements.txt`

### 常见运行错误

