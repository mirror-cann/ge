# 算子库包版本问题导致加载单算子失败

## 问题现象

加载单算子报错失败，日志显示如下类似信息：

```console
E19999: Inner Error
E19999 The opp version of the model does not match the current opp run package, Model is [6.4.T11.0.B300], opp run package is [7.0.T3.0.B107], try to convert the om again!
```

## 可能原因

动态Shape算子场景下或者昇腾虚拟化实例场景下，单算子模型数据加载环境中的算子库包安装版本（包名为Ascend-cann-toolkit_\_\{version\}_\_linux-_\{arch\}_.run）与om模型文件编译环境的**算子库包安装版本不一致**，导致加载算子时会报错。

## 解决方法

动态Shape算子场景下或者昇腾虚拟化实例场景下，单算子模型数据加载环境中的算子库包安装版本需与om模型文件编译环境的**算子库包安装版本保持一致**，出现该报错后，需排查安装版本，选择更换算子加载环境的opp包版本或更换编译算子om文件环境的opp包版本，若选择更换后者，则需要重新转换模型。
