# CustomPassFn

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

注册自定义Pass的执行函数。

关于接口的详细使用方法请参见编程指南 \> 自定义Pass开发 \> 使用自定义Pass修改Graph。

## 函数原型

```c++
PassRegistrationData &CustomPassFn(const CustomPassFunc &custom_pass_fn)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| custom_pass_fn | 输入 | 自定义Pass的执行函数。 |

## 返回值说明

返回自身对象的引用。

## 约束说明

无

## 回调函数CustomPassFunc

用户自定义并实现CustomPassFunc类函数，即自定义的改图函数。

```c++
Status CustomPassFunc(GraphPtr &graph, CustomPassContext &custom_context)
```

**表 1**  参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 要修改的图。 |
| custom_context | 输入 | 维测类对象，通过此对象向框架注册维测信息。 |
| - | 输出 | - SUCCESS：成功。<br>  - 其他值：失败。 |
