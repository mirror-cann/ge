# --status\_check

## 产品支持情况

<!-- npu="950" id2 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id2 -->

<!-- npu="A3" id3 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id3 -->

<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id4 -->

<!-- npu="310b" id5 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id5 -->

<!-- npu="310p" id6 -->
- Atlas 推理系列产品：支持
<!-- end id6 -->

<!-- npu="910" id7 -->
- Atlas 训练系列产品：支持
<!-- end id7 -->

<!-- npu="IPV350" id1 -->
- IPV350：支持
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--status_check_res.md#id1 -->

## 功能说明

控制编译算子时是否插入溢出检测逻辑。

若模型计算精度异常且怀疑由算子溢出导致，可在编译阶段启用此参数以添加溢出检测逻辑，随后重新编译模型进行排查。

## 关联参数

- 使用该参数时，建议与[--op\_debug\_level](--op_debug_level.md)参数配合使用，这样在生成的算子\*.cce文件中，可以查看是否加入了溢出检测逻辑，加入了溢出检测逻辑的代码样例如下：

    ```c++
      if (status_overflow[0]) {
        xxxxxx
    }
    ```

## 参数取值

- 0：（默认值）不开启溢出检测，算子编译时不插入溢出检测逻辑。
- 1：启用溢出检测，编译算子时插入溢出检查逻辑。

## 示例

```bash
atc --status_check=1 ...
```

## 使用约束

使用[--status\_check](--status_check.md)参数只是在模型编译后生成的算子\*.cce文件中加入了溢出检测逻辑，如果想查看具体哪些算子有溢出，则需要配合模型推理过程中提供的**aclInit**接口，在该接口入参的JSON配置文件中打开“dump\_debug”开关，接口详细说明请参见[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclinit)中的“初始化和去初始化 \> aclInit”章节。
