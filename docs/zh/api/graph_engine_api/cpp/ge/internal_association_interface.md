# 内部关联接口

ge\_api\_error\_codes.h中的如下接口是内部关联接口，开发者不需要直接调用：

- ErrorNoRegisterar类：错误码注册类，用于注册具体的错误码及其描述。

    包含以下成员函数：

    - 2个构造函数：

        ErrorNoRegisterar\(uint32\_t err, const std::string &desc\) noexcept

        ErrorNoRegisterar\(const uint32\_t err, const char \*const desc\) noexcept

        其中，err：错误码；desc：错误码描述

    - 1个默认析构函数：

        \~ErrorNoRegisterar\(\) = default

- StatusFactory类：单例状态工厂类，管理注册的错误码。

    包含以下成员函数：

    - AscendString GetErrDescV2\(const uint32\_t err\)

        根据错误值获取错误码描述（abi兼容接口，推荐优先使用）。

    - static StatusFactory \*Instance\(\)

        单例返回StatusFactory类对象，全局唯一。

    - void RegisterErrorNo\(uint32\_t err, const std::string &desc\)

        void RegisterErrorNo\(const uint32\_t err, const char \*const desc\)

        用于错误码注册。

    - std::string GetErrDesc\(uint32\_t err\)

        根据错误码值获取错误码描述。
