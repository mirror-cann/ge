# RegisterCallBackFunc

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

注册回调函数。

注册用户指定的Summary、Checkpoint回调接口。当用户下发给GE的图中带有Summary、Checkpoint算子时，GE会调用该回调函数。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
Status RegisterCallBackFunc(const std::string &key, const pCallBackFunc &callback)
Status RegisterCallBackFunc(const char *key, const session::pCallBackFunc &callback)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| key | 输入 | 注册回调函数对应的关键字，字符串或者字符格式，表示回调函数类型，支持"Summary"、"Save"。 |
| callback | 输入 | 回调函数返回的对应信息。详情请参见表格下面的内容。 |

```c++
typedef uint32_t (*pCallBackFunc)(uint32_t graph_id, const std::map<std::string, ge::Tensor> &params_list);
typedef uint32_t (*pCallBackFunc)(uint32_t graph_id, const std::map<AscendString, ge::Tensor> &params_list);
```

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_SESSION_MANAGER_NOT_INIT：Session管理未初始化。<br>SUCCESS：注册回调函数成功。<br>FAILED：注册回调函数失败。 |

## 约束说明

- 回调函数类型仅支持Summary、Save。
- 如无注册则下发Summary、Checkpoint算子会报错。
- 目前暂时只支持图执行完后一次性调用回调函数。
