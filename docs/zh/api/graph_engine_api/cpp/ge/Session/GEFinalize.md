# GEFinalize

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

GE退出，释放GE相关资源。

在该接口内，默认增加2000ms延时（实际最大延时可达2000ms），用于Device业务日志回传，保证ERROR级别和EVENT级别日志不丢失，您可以将**ASCEND\_LOG\_DEVICE\_FLUSH\_TIMEOUT**环境变量设置为0（命令示例：**export ASCEND\_LOG\_DEVICE\_FLUSH\_TIMEOUT=0**），去除该默认延时。

关于**ASCEND\_LOG\_DEVICE\_FLUSH\_TIMEOUT**环境变量的详细描述请参见《环境变量参考》。

## 函数原型

```c++
Status GEFinalize()
```

## 参数说明

无

## 返回值说明

无

## 约束说明

无
