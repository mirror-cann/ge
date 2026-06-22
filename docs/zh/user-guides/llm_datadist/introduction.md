# 简介

LLM-DataDist作为大模型分布式集群和数据管理组件，提供了高性能、零拷贝的点对点KV传输的能力，该能力通过简易的API开放给用户。
本文档提供开发指南，用于指导开发者如何使用LLM-DataDist接口实现集群间的数据传输，构建大模型推理分离式框架。

**表1** 使用场景介绍

|手册名称|简介|
|--|--|
|LLM DataDist开发(C++)|介绍通过C++的LLM-DataDist相关接口，如何进行链路管理和KV Cache管理。该场景下支持单边建链，即Client向Server发起建链。数据传输限制只能从Decode往Prompt拉取KV，从Prompt往Decode推送KV。该场景下仅D2D传输。|
|LLM DataDist开发(Python)|介绍通过Python的LLM-DataDist相关接口，在KvCacheManager模式下如何进行链路管理和KV Cache管理。该场景下支持单边建链。数据传输限制只能从Decode往Prompt拉取KV，只能从Prompt往Decode推送KV。该场景下仅D2D传输。|
