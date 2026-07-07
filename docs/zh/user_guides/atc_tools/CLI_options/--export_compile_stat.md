# --export\_compile\_stat

## 产品支持情况

全量芯片支持

## 功能说明

配置图编译过程中**是否生成**算子融合信息（包括图融合和UB融合）的结果文件fusion\_result.json。

该文件用于记录图编译过程中使用的融合规则，文件中：

- session\_and\_graph\_id\__xx\_xx_：表示融合结果所属线程和图编号。
- graph\_fusion：表示图融合。
- ub\_fusion：表示UB融合

    <!-- npu="950" id1 -->
    **Ascend 950PR/Ascend 950DT不支持UB融合，不会生成该信息**。
    <!-- end id1 -->

- match\_times：表示图编译过程中匹配到的融合规则次数。
- effect\_times：表示实际生效的次数。
- repository\_hit\_times：优化UB融合知识库命中的次数

    <!-- npu="950" id2 -->
    **Ascend 950PR/Ascend 950DT不支持UB融合，不会生成该信息**。
    <!-- end id2 -->

## 关联参数

该参数用于生成算子融合信息，而[--fusion\_switch\_file](--fusion_switch_file.md)参数可以关闭指定的融合规则，关闭的融合规则，不会在fusion\_result.json文件中呈现。

## 参数取值

- 0：不生成算子融合信息结果文件。
- 1：（默认值）程序运行正常退出时，生成算子融合信息结果文件。
- 2：图编译完成时，生成算子融合信息结果文件。即如果图编译已完成，后续程序提前中断，也会生成算子融合信息结果文件。

若未设置ASCEND\_WORK\_PATH环境变量，结果文件默认生成在执行atc命令的当前路径；若设置了ASCEND\_WORK\_PATH环境变量，则保存路径为：$ASCEND\_WORK\_PATH/FE/$\{进程号\}/fusion\_result.json。环境变量详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

## 示例

```bash
atc --export_compile_stat=1 ...
```

## 使用约束

无。
