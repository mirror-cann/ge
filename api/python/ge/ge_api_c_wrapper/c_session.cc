/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl/acl_rt.h"
#include "graph/graph.h"
#include "utils/graph_utils.h"
#include "graph/tensor.h"
#include "ge/ge_allocator.h"
#include "ge/ge_api.h"
#include "utils/graph_utils_ex.h"
#include "common/checker.h"
#include "ge_api_c_wrapper_utils.h"
#include "common/model/external_allocator_manager.h"

#include <memory>
#include <mutex>

#ifdef __cplusplus
using namespace ge;
using namespace ge::c_wrapper;
using Status = uint32_t;
using Tensor = ge::Tensor;
namespace {
using PyMallocFunc = void *(*)(void *obj, size_t size);
using PyFreeFunc = void (*)(void *obj, void *block);
using PyGetAddrFunc = void *(*)(void *block);
using PyOnDestroyFunc = void (*)(void *prevent_gc_handle);

struct PyCallbackAllocator : public ge::Allocator, public std::enable_shared_from_this<PyCallbackAllocator> {
  PyMallocFunc malloc_func = nullptr;
  PyFreeFunc free_func = nullptr;
  PyGetAddrFunc get_addr_func = nullptr;
  PyOnDestroyFunc on_destroy_func = nullptr;
  void *prevent_gc_handle = nullptr;
  std::mutex mutex;

  ge::MemBlock *Malloc(size_t size) override;
  void Free(ge::MemBlock *mb) override;

  ~PyCallbackAllocator() override {
    if (on_destroy_func != nullptr) {
      on_destroy_func(prevent_gc_handle);
    }
  }
};

class PyCallbackMemBlock : public ge::MemBlock {
 public:
  PyCallbackMemBlock(const std::shared_ptr<PyCallbackAllocator> &owner, void *addr, size_t size, void *py_handle)
      : ge::MemBlock(*owner, addr, size), owner_(owner), py_handle_(py_handle) {}
 private:
  std::shared_ptr<PyCallbackAllocator> owner_;
 public:
  void *py_handle_;
};

ge::MemBlock *PyCallbackAllocator::Malloc(size_t size) {
  const std::lock_guard<std::mutex> lk(mutex);
  auto *block = malloc_func(nullptr, size);
  if (block == nullptr) {
    return nullptr;
  }
  void *addr = get_addr_func(block);
  return new (std::nothrow) PyCallbackMemBlock(shared_from_this(), addr, size, block);
}

void PyCallbackAllocator::Free(ge::MemBlock *mb) {
  auto *pcmb = dynamic_cast<PyCallbackMemBlock *>(mb);
  if (pcmb == nullptr) {
    return;
  }
  {
    const std::lock_guard<std::mutex> lk(mutex);
    free_func(nullptr, pcmb->py_handle_);
  }
  delete pcmb;
}

struct DefaultMemBlock {
  void *addr;
  size_t size;
};

void *DefaultMalloc(void * /*obj*/, size_t size) {
  void *ptr = nullptr;
  if (aclrtMalloc(&ptr, size, ACL_MEM_MALLOC_NORMAL_ONLY) != ACL_SUCCESS) {
    return nullptr;
  }
  auto *block = new (std::nothrow) DefaultMemBlock{ptr, size};
  if (block == nullptr) {
    (void)aclrtFree(ptr);
    return nullptr;
  }
  return block;
}

void DefaultFree(void * /*obj*/, void *block) {
  auto *mb = static_cast<DefaultMemBlock *>(block);
  if (mb != nullptr && mb->addr != nullptr) {
    (void)aclrtFree(mb->addr);
  }
  delete mb;
}

void *DefaultGetAddr(void *block) {
  auto *mb = static_cast<DefaultMemBlock *>(block);
  return mb != nullptr ? mb->addr : nullptr;
}
}  // namespace
extern "C" {
#endif

Session *GeApiWrapper_Session_CreateSession() {
  std::map<ge::AscendString, ge::AscendString> options;
  auto *session = new (std::nothrow) ge::Session(options);
  return session;
}

Session *GeApiWrapper_Session_CreateSessionWithOptions(char **keys, char **values, int size) {
  GE_ASSERT_NOTNULL(keys);
  GE_ASSERT_NOTNULL(values);
  std::map<AscendString, AscendString> options;
  for (int i = 0; i < size; i++) {
    GE_ASSERT_NOTNULL(keys[i]);
    GE_ASSERT_NOTNULL(values[i]);
    options.emplace(keys[i], values[i]);
  }
  auto *session = new (std::nothrow) ge::Session(options);
  return session;
}

Status GeApiWrapper_Session_AddGraph(Session *session, uint32_t graph_id, Graph *graph) {
  GE_ASSERT_NOTNULL(session);
  GE_ASSERT_NOTNULL(graph);
  return session->AddGraph(graph_id, *graph);
}

Status GeApiWrapper_Session_AddGraphWithOptions(Session *session, uint32_t graph_id, Graph *graph, char **keys, char **values, int size) {
  GE_ASSERT_NOTNULL(session);
  GE_ASSERT_NOTNULL(graph);
  GE_ASSERT_NOTNULL(keys);
  GE_ASSERT_NOTNULL(values);
  std::map<AscendString, AscendString> options;
  for (int i = 0; i < size; i++) {
    GE_ASSERT_NOTNULL(keys[i]);
    GE_ASSERT_NOTNULL(values[i]);
    options.emplace(keys[i], values[i]);
  }
  return session->AddGraph(graph_id, *graph, options);
}

Status GeApiWrapper_Session_RemoveGraph(Session *session, uint32_t graph_id) {
  GE_ASSERT_NOTNULL(session);
  return session->RemoveGraph(graph_id);
}

Tensor** GeApiWrapper_Session_RunGraph(Session *session, uint32_t graph_id, void **inputs, int input_count, size_t *tensor_num) {
  GE_ASSERT_NOTNULL(session);
  GE_ASSERT_NOTNULL(inputs);
  GE_ASSERT_NOTNULL(tensor_num);
  std::vector<Tensor> inputs_vector;
  for (int i = 0; i < input_count; i++) {
    auto *tn = static_cast<Tensor *>(inputs[i]);
    inputs_vector.push_back(*tn);
  }
  std::vector<Tensor> outputs_vector;
  GE_ASSERT_GRAPH_SUCCESS(session->RunGraph(graph_id, inputs_vector, outputs_vector));
  return VecTensorToArray(outputs_vector, tensor_num);
}

Tensor** GeApiWrapper_Session_RunGraphWithStreamAsync(Session *session, uint32_t graph_id, void *stream, void **inputs,
                                                      int input_count, size_t *tensor_num) {
  GE_ASSERT_NOTNULL(session);
  GE_ASSERT_NOTNULL(stream);
  GE_ASSERT_NOTNULL(inputs);
  GE_ASSERT_NOTNULL(tensor_num);
  std::vector<Tensor> inputs_vector;
  inputs_vector.reserve(static_cast<size_t>(input_count));
  for (int i = 0; i < input_count; i++) {
    GE_ASSERT_NOTNULL(inputs[i]);
    auto *tn = static_cast<Tensor *>(inputs[i]);
    inputs_vector.push_back(*tn);
  }

  std::vector<Tensor> outputs_vector;
  GE_ASSERT_GRAPH_SUCCESS(session->RunGraphWithStreamAsync(graph_id, stream, inputs_vector, outputs_vector));
  return VecTensorToArray(outputs_vector, tensor_num);
}

void GeApiWrapper_Session_DestroySession(const Session *session) {
  delete session;
}

Status GeApiWrapper_Session_RegisterDefaultAllocator(Session *session, void *stream) {
  GE_ASSERT_NOTNULL(session);
  GE_ASSERT_NOTNULL(stream);
  auto allocator = std::make_shared<PyCallbackAllocator>();
  allocator->malloc_func = DefaultMalloc;
  allocator->free_func = DefaultFree;
  allocator->get_addr_func = DefaultGetAddr;
  return session->RegisterExternalAllocator(stream, allocator);
}

Status GeApiWrapper_Session_RegisterExternalAllocator(
    Session *session, void *stream,
    PyMallocFunc malloc_fn, PyFreeFunc free_fn,
    PyGetAddrFunc get_addr_fn,
    PyOnDestroyFunc on_destroy_fn, void *prevent_gc_handle) {
  GE_ASSERT_NOTNULL(session);
  GE_ASSERT_NOTNULL(stream);
  GE_ASSERT_NOTNULL(malloc_fn);
  GE_ASSERT_NOTNULL(free_fn);
  GE_ASSERT_NOTNULL(get_addr_fn);
  auto allocator = std::make_shared<PyCallbackAllocator>();
  allocator->malloc_func = malloc_fn;
  allocator->free_func = free_fn;
  allocator->get_addr_func = get_addr_fn;
  allocator->on_destroy_func = on_destroy_fn;
  allocator->prevent_gc_handle = prevent_gc_handle;
  return session->RegisterExternalAllocator(stream, allocator);
}

Status GeApiWrapper_Session_UnregisterExternalAllocator(Session *session, void *stream) {
  GE_ASSERT_NOTNULL(session);
  GE_ASSERT_NOTNULL(stream);
  return session->UnregisterExternalAllocator(stream);
}

bool GeApiWrapper_HasExternalAllocator(void *stream) {
  return ge::ExternalAllocatorManager::GetExternalAllocator(stream) != nullptr;
}

#ifdef __cplusplus
}
#endif
