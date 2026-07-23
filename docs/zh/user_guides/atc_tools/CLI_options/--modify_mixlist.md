# --modify\_mixlist

## 产品支持情况

全量芯片支持

## 功能说明

混合精度场景下，**修改**算子使用的混合精度黑白灰名单，自行指定哪些算子允许降精度，哪些算子不允许降精度。

黑白灰名单，可从`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数下的flag参数值：（其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。*xxx*请根据实际产品进行选择。）

- 若取值为true（白名单），表示混合精度模式下，**允许**降低精度。
- 若取值为false（黑名单），表示混合精度模式下，**不允许**降低精度。
- 不配置该参数（灰名单），表示混合精度模式下，当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

## 关联参数

开启混合精度方式：

- [--precision\_mode](--precision_mode.md)参数设置为allow\_mix\_precision、allow\_mix\_precision\_fp16、allow\_mix\_precision\_bf16。
- [--precision\_mode\_v2](--precision_mode_v2.md)参数设置为mixed\_float16、mixed\_bfloat16、mixed\_hif8。

    与[--precision\_mode](--precision_mode.md)参数不能同时配置，建议使用[--precision\_mode\_v2](--precision_mode_v2.md)。

    <!-- npu="A3,910b,310b" id5 -->
    bf16仅在如下产品型号支持：
    <!-- end id5 -->

    <!-- npu="A3" id1 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id1 -->

    <!-- npu="910b" id2 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id2 -->

    <!-- npu="310b" id3 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id3 -->

    <!-- npu="950" id4 -->
    hif8仅在Ascend 950PR/Ascend 950DT支持。
    <!-- end id4 -->

    <!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--modify_mixlist_res.md#id1 -->

## 参数取值

**参数值：**
混合精度名单路径以及文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束**：

- 名单格式为\*.json格式，文件中的算子列表由用户指定，多个算子使用英文逗号分隔。
- 配置的算子类型必须为基于Ascend IR定义的算子的OpType，算子类型查看方法请参见[如何确定原始框架网络模型中的算子与AI处理器支持的算子的对应关系](../FAQ/operator_correspondence_guide.md)。

## 推荐配置及收益

无。

## 示例

黑白灰名单查询示例如下，flag参数值为true表示白名单，为false表示黑名单，不配置flag参数表示灰名单：

```json
"Conv2D":{
    ......
    "precision_reduce":{
        "flag":"true"
     },
    ......
}
```

混合精度名单样例如下，_ops\_info.json为文件名示例_，_OpTypeA_、_OpTypeB_、_OpTypeC_、_OpTypeD为算子示例_。

```json
{
  "black-list": {                  // 黑名单
     "to-remove": [                // 黑名单算子转换为灰名单算子，配置该参数时，请确保被转换的算子已经存在于黑名单中
     "OpTypeA"
     ],
     "to-add": [                   // 白名单或灰名单算子转换为黑名单算子
     "OpTypeB"
     ]
  },
  "white-list": {                  // 白名单
     "to-remove": [                // 白名单算子转换为灰名单算子，配置该参数时，请确保被转换的算子已经存在于白名单中
     "OpTypeC"
     ],
     "to-add": [                   // 黑名单或灰名单算子转换为白名单算子
     "OpTypeD"
     ]
  }
}
```

- 假设算子A默认在白名单中，如果您希望将该算子配置为黑名单算子，则配置示例和系统处理逻辑为：
    1. 将该算子添加到黑名单中：

        ```json
        {
          "black-list": {
             "to-add": ["A"]
          }
        }
        ```

        则系统会将该算子从白名单中删除，并添加到黑名单中，最终该算子在黑名单中。

    2. 将该算子从白名单中删除，同时添加到黑名单中：

        ```json
        {
          "black-list": {
             "to-add": ["A"]
          }
          "white-list": {
             "to-remove": ["A"]
          }
        }
        ```

        则系统会将该算子从白名单中删除，并添加到黑名单中，最终该算子在黑名单中。

- 对于只从黑/白名单中删除，而不添加到白/黑名单的场景，系统会将该算子添加到灰名单中，配置示例如下（例如，从白名单删除某个算子）：

    ```json
    {
      "white-list": {
         "to-remove": ["A"]
      }
    }
    ```

    则系统会将该算子从白名单中删除，然后添加到灰名单中，最终该算子在灰名单中。

将配置好的_ops\_info.json_文件上传到ATC工具所在服务器任意目录，例如上传到$HOME/module，使用示例如下：

```bash
atc --precision_mode=allow_mix_precision  --modify_mixlist=$HOME/module/ops_info.json ...
```

## 依赖约束

无。
