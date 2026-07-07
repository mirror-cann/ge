# 使用AIPP色域转换模型时如何判断视频流的格式标准

## 问题现象描述

使用AIPP色域转换模型时无法判断视频流的格式标准。

## 原因分析

AIPP中色域转换要求的是视频流格式为BT-709WIDE、BT-709NARROW、BT-601NARROW、BT-601WIDE这几种，但是对于用户来说，一段视频流，往往不确定属于哪个格式，从而无法确定使用哪种色域转换模板。

## 解决措施

此处以第三方**ffprobe**工具为例，演示如何进行判断。

1. 从官方路径下载工具及相关文档：[https://www.ffmpeg.org/ffprobe-all.html#Description](https://www.ffmpeg.org/ffprobe-all.html#Description)。
2. 通过**ffprobe -show\_frames   _filename_**参数获取对应视频信息。****

    **参数功能：**Show information about each frame and subtitle contained in the input multimedia stream. The information for each single frame is printed within a dedicated section with name "FRAME" or "SUBTITLE".

3. 通过结果中如下信息确认是哪种视频标准。

    - "color\_range"："tv"或者"pc"。
    - "color\_space"："bt709"或者"bt601"。

    其中tv表示limited，即narrow range。pc表示full，即wide range。

    例如查询结果为color\_range=tv，color\_space=bt709，则代表BT-709NARROW。

    > [!NOTE]说明
    >如命令变化，请以工具官方说明为准。
