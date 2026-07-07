# aclGetCompileopt

## 产品支持情况

<!-- npu="950" id706 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id706 -->
<!-- npu="A3" id707 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id707 -->
<!-- npu="910b" id708 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id708 -->
<!-- npu="310b" id709 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id709 -->
<!-- npu="310p" id710 -->
- Atlas 推理系列产品：支持
<!-- end id710 -->
<!-- npu="910" id711 -->
- Atlas 训练系列产品：支持
<!-- end id711 -->
<!-- npu="IPV350" id712 -->
- IPV350：不支持
<!-- end id712 -->

## 功能说明

用户可调用本接口获取编译选项的值。

用户可以调用本接口获取当前编译选项的值，在调用[aclSetCompileopt](aclSetCompileopt.md)前，本接口返回的是各编译选项的默认值；在调用[aclSetCompileopt](aclSetCompileopt.md)后，本接口返回的是用户设置的值。

## 函数原型

```c
aclError aclGetCompileopt(aclCompileOpt opt, char *value, size_t length)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| opt | 输入 | 编译选项。类型定义请参见[aclCompileOpt](aclCompileOpt.md)。 |
| value | 输出 | 存放编译选项值的内存指针。<br>用户在调用本接口前需要申请一块内存来保存编译选项的值，内存大小可以通过[aclGetCompileoptSize](aclGetCompileoptSize.md)获得。 |
| length | 输入 | value指向内存的大小，单位：Byte。 |

## 返回值说明

返回0表示成功，返回其他值表示失败，请参见[aclError](aclError.md)。
