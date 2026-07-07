# --log

## 产品支持情况

全量芯片支持

## 功能说明

设置ATC模型转换过程中日志的级别。

## 关联参数

无。

## 参数取值

**调试日志**，支持设置如下级别：

- debug：输出debug/info/warning/error级别的调试日志信息。
- info：输出info/warning/error级别的调试日志信息。
- warning：输出warning/error级别的调试日志信息。
- error：输出error级别的调试日志信息。
- null：（默认值）不输出调试日志。

**运行日志**默认会输出info/warning/error/event级别日志，不支持级别调整。**安全日志**默认输出debug/info/warning/error级别日志，不支持级别调整。

## 推荐配置及收益

无。

## 示例

```bash
atc --log=debug ...
```

如果模型转换失败，则可以通过分析日志定位问题。日志格式如下，更多日志信息请参见《日志参考》。
<!-- npu="IPV350" id1 -->
IPV350不支持该手册中的特性。
<!-- end id1 -->

```console
[Level] ModuleName(PID,PName):DateTimeMS [FileName:LineNumber]LogContent
```

各字段解释如下：

**表 1**  日志字段说明

| 字段 | 说明 |
| --- | --- |
| Level | 日志级别。调试日志存在4种日志级别：ERROR、WARNING、INFO、DEBUG。 |
| ModuleName | 产生日志的模块的名称。 |
| PID | 进程ID。 |
| PName | 进程名称。 |
| DateTimeMS | 日志打印时间，格式为：yyyy-mm-dd-hh:mm:ss.SSS.SSS。 |
| FileName:LineNumber | 调用日志打印接口的文件及对应的行号。 |
| LogContent | 各模块具体的日志内容。 |

样例如下：

```console
[INFO] FE(30741,atc.bin):2021-12-09-16:10:22.539.141 [fe_type_utils.cc:52]30741 GetRealPath:"path /usr/local/Ascend/opp/built-in/op_impl/ai_core/tbe/config/ascendxxx is not exist."
[WARNING] FE(30741,atc.bin):2021-12-09-16:10:22.539.146 [sub_op_info_store.cc:52]30741 Initialize:"The config file[/usr/local/Ascend/opp/built-in/op_impl/ai_core/tbe/config/ascendxxx] of op information library[tbe-builtin] is not existed. "
[ERROR] GE(30741,atc.bin):2021-12-09-16:10:22.539.201 [error_manager.cc:263]30741 ReportErrMessage: [INIT][OPS_KER][Report][Error]error_code: W21000, arg path is not existed in map
```

问题定位思路：

**表 2**  问题定位思路

| 字段 | 说明 | 解决思路 |
| --- | --- | --- |
| GE | GE图编译或校验问题。 | 校验类报错，通常会给出明确的错误原因，此时需要针对性的修改模型转换使用的参数，以满足相关要求。 |
| FE | 算子融合问题。 | 无。 |
| TEFUSION | - 算子预编译/编译问题。<br>  - 融合算子编译问题。 | 常见错误信息以及解决思路：<br><br>  1. ModuleNotFoundError: No module named 'decorator'解决思路：根据提示信息安装pip包。<br>  2. ModuleNotFoundError: No module named 'te'解决思路：安装ATC工具所在软件包时，安装命令没有使用--pylocal，建议使用该参数重新安装相应软件包。 |
| TBE | 算子编译问题。 | 无。 |

## 依赖约束

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
- **日志落盘**：

    atc命令执行过程中，日志默认落盘到如下路径：

  - $HOME/ascend/log/**debug**/plog/plog-_pid_\_\*.log：调试日志。

    调试日志场景，由于[--log](--log.md)默认值为null，即不输出日志，若上述路径存在日志信息，则为atc进程之外的其他日志信息，比如依赖Python相关信息；若想要日志体现atc进程相关信息，则[--log](--log.md)设置为除null以外的其他取值。

  - $HOME/ascend/log/**run**/plog/plog-_pid_\_\*.log：运行日志。
  - $HOME/ascend/log/**security**/plog/plog-_pid_\_\*.log：安全日志。

    pid代表进程ID，“\*”表示该日志文件创建时的时间戳。
  <!-- end id2 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--log_res.md#id1 -->

<!-- npu="950,A3,910b,910,310p,310b" id3 -->
- **日志打屏**：

    atc命令执行过程中，日志默认不打屏，如需打屏显示，则请：

  - 在执行atc命令的当前窗口设置打屏环境变量：

    ```bash
     export ASCEND_SLOG_PRINT_TO_STDOUT=1
    ```

  - atc模型转换命令中，设置[--log](--log.md)参数（不能设置为null）。

    关于日志的更多信息请参见[《日志参考》](https://hiascend.com/document/redirect/CannCommunitylogref)。
<!-- end id3 -->

- **日志重定向**：

    如果不想日志落盘，而是重定向到文件，则模型转换前需要设置上述的日志打屏环境变量，并且atc命令需要设置[--log](--log.md)参数（不能设置为null），样例如下：

    ```bash
    atc xxx --log=debug >log.txt
    ```

    _xxx_请替换为实际参数。
