# MatchNext

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern\_matcher.h\>
- 库文件：libge\_compiler.so

## 功能说明

按照当前Target Graph的拓扑顺序，返回下一个符合Pattern定义的匹配结果。

提供逐一匹配接口可解决Overlap场景下的选择问题。用户可循环调用该接口。

说明：

- 当前接口返回的匹配结果可能存在重叠，调用者需要自己处理

    考虑如下结构，当对ABC做匹配时，C会先后出现在2个匹配结果中，选取哪个结果需要调用者自行决定。

    ```mermaid
             B  C   B
             \ / \ /
              A   A
    ```

- 该函数只匹配当前图，若Target Graph带子图，调用者需自行对子图进行匹配。

## 函数原型

```c++
std::unique_ptr<MatchResult> MatchNext()
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr\<MatchResult> | 匹配结果。 |

## 约束说明

无
