# aclOpCompileFlag

**该参数已废弃，请勿配置，否则后续版本可能存在兼容性问题。**

```c
typedef enum aclCompileFlag {
    ACL_OP_COMPILE_DEFAULT,  //精确编译，不设置编译flag时，默认是精确编译
    ACL_OP_COMPILE_FUZZ
} aclOpCompileFlag;
```
