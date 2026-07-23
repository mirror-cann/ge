# --oo\_level

## 产品支持情况

全量芯片支持

## 功能说明

**调试功能扩展参数，暂不支持应用于生产环境，后续版本会作为正式功能更新发布。**

图编译多级优化选项，包括子图优化、整图优化、静态Shape模型下沉等。

静态Shape模型下沉：静态Shape模型在编译时即可确定所有算子的输入输出Shape，完成模型级内存编排、算子的Tiling计算等Host侧计算，在模型加载时整体下发到Device流上，但不立即执行，通过下发模型执行Task触发模型中所有Task的执行。

## 关联参数

无。

## 参数取值

- O1：会关闭所有图融合和UB融合Pass，只做促成静态下沉的相关优化，如InferShape（进行输出Tensor的shape推导）、常量折叠、死边消除等。

    <!-- npu="950" id1 -->
    Ascend 950PR/Ascend 950DT不支持UB融合，只关闭图融合Pass。
    <!-- end id1 -->

- O3：（默认值）开启所有优化。

## 推荐配置及收益

无。

## 示例

```bash
atc --oo_level=O1 ...
```

## 使用约束

取值为O1时，会关闭所有图融合和UB融合PASS，只开启静态下沉的相关PASS，但是如下路径文件中的图融合PASS，由于关闭后会有功能问题，会默认开启：

“$\{INSTALL\_DIR\}/x86\_64-linux/lib64/plugin/opskernel/fusion\_pass/config/fusion\_config.json”文件中"ExceptionalPassOfO1Level"字段下的所有图融合PASS。

其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。
