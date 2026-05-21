/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "folding.h"

#include <vector>
#include <set>
#include <string>
#include <unordered_map>
#include <dirent.h>
#include <dlfcn.h>
#include <cstring>
#include "cpu_kernel_register.h"
#include "cpu_kernel.h"
#include "cpu_context.h"
#include "proto/aicpu/cpu_attr.pb.h"
#include "proto/aicpu/cpu_node_def.pb.h"
#include "proto/aicpu/cpu_tensor.pb.h"
#include "proto/aicpu/cpu_tensor_shape.pb.h"
#include "util/log.h"
#include "graph/types.h"
#include "mmpa/mmpa_api.h"

namespace {
const char* const kVtString = "VT_STRING";
const char* const kVtListString = "VT_LIST_STRING";
const char* const kVtFloat = "VT_FLOAT";
const char* const kVtListFloat = "VT_LIST_FLOAT";
const char* const kVtInt = "VT_INT";
const char* const kVtListInt = "VT_LIST_INT";
const char* const kVtListListInt = "VT_LIST_LIST_INT";
const char* const kVtBool = "VT_BOOL";
const char* const kVtListBool = "VT_LIST_BOOL";
const char* const kVtDataType = "VT_DATA_TYPE";
const char* const kVtListDataType = "VT_LIST_DATA_TYPE";
const char* const kVtTensor = "VT_TENSOR";
const char* const kVtListTensor = "VT_LIST_TENSOR";
const char kPathSeparator = '/';
const char* const kConstantFoldingSoPrefix = "lib";
const char* const kConstantFoldingSoSuffix = "constant_folding_ops.so";
const char* const kExcludedConstantFoldingSo = "libconstant_folding_ops.so";
const char* const kSymGetAllRegisteredOpTypesV2 = "GetAllRegisteredOpTypesV2";
const char* const kSymIsRegisteredV2 = "IsRegisteredV2";
const char* const kSymRunCpuKernelV2 = "RunCpuKernelV2";

using AttrValueMap = google::protobuf::Map<string, aicpuops::AttrValue>;

using GetAllRegisteredOpTypesV2Fn = std::vector<std::string>(*)();
using IsRegisteredV2Fn = bool(*)(const std::string &);
using RunCpuKernelV2Fn = uint32_t(*)(aicpu::CpuKernelContext &);

struct V2ModuleBinding {
  void *handle = nullptr;
  GetAllRegisteredOpTypesV2Fn get_all_op_types = nullptr;
  IsRegisteredV2Fn is_registered = nullptr;
  RunCpuKernelV2Fn run_cpu_kernel = nullptr;
  std::string so_name;
};

std::vector<V2ModuleBinding> g_v2_bindings;
// op_type->binding反向索引, Init阶段一次性构建, 运行期只读。
std::unordered_map<std::string, const V2ModuleBinding *> g_v2_op_index;

void ConvertGeToAicpuTensor(const ge::GeTensorDesc &tensor_desc,
                            const std::string &tensor_name,
                            const ge::Tensor &ge_tensor,
                            aicpuops::Tensor *aicpu_tensor) {
  aicpu_tensor->set_name(tensor_name);
  aicpu_tensor->set_tensor_type(tensor_desc.GetDataType());
  aicpu_tensor->set_data_ptr(static_cast<uint64_t>(reinterpret_cast<intptr_t>(ge_tensor.GetData())));
  aicpu_tensor->set_data_size(static_cast<uint64_t>(ge_tensor.GetSize()));
  auto shape = aicpu_tensor->mutable_tensor_shape();
  if (shape != nullptr) {
    shape->clear_dim();
    std::vector<int64_t> dims = tensor_desc.GetShape().GetDims();
    for (size_t i = 0; i < dims.size(); i++) {
      aicpuops::TensorShape_Dim *aicpu_dims = shape->add_dim();
      if (aicpu_dims != nullptr) {
        aicpu_dims->set_size(dims[i]);
      }
    }
    shape->set_data_format(static_cast<ge::Format>(tensor_desc.GetFormat()));
  }
  AICPUE_LOGI("Op set tensor[%s], tensor info[type:%d, data:%p, size:%llu].",
         tensor_name.c_str(), static_cast<int>(tensor_desc.GetDataType()),
         ge_tensor.GetData(), ge_tensor.GetSize());
}

int32_t AddStringAttrToNodeDef(const ge::Operator &op, const char *name,
                               [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::string s;
  ge::graphStatus ret = op.GetAttr(name, s);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  attr_value.set_s(s);

  AICPUE_LOGD("Finish add string attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddListStringAttrToNodeDef(const ge::Operator &op, const char *name,
                                   [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::vector<std::string> list_s;
  ge::graphStatus ret = op.GetAttr(name, list_s);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto array = attr_value.mutable_array();
  if (array == nullptr) {
    return -1;
  }

  for (std::string value : list_s) {
    array->add_s(value);
  }

  AICPUE_LOGD("Finish add list string attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddFloatAttrToNodeDef(const ge::Operator &op, const char *name,
                              [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  float f = 0;
  ge::graphStatus ret = op.GetAttr(name, f);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  attr_value.set_f(f);

  AICPUE_LOGD("Finish add float attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddListFloatAttrToNodeDef(const ge::Operator &op, const char *name,
                                  [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::vector<float> list_f;
  ge::graphStatus ret = op.GetAttr(name, list_f);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto array = attr_value.mutable_array();
  if (array == nullptr) {
    return -1;
  }

  for (float value : list_f) {
    array->add_f(value);
  }

  AICPUE_LOGD("Finish add list float attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddBoolAttrToNodeDef(const ge::Operator &op, const char *name,
                             [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  bool b = false;
  ge::graphStatus ret = op.GetAttr(name, b);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  attr_value.set_b(b);

  AICPUE_LOGD("Finish add bool attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddListBoolAttrToNodeDef(const ge::Operator &op, const char *name,
                                 [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::vector<bool> list_b;
  ge::graphStatus ret = op.GetAttr(name, list_b);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto array = attr_value.mutable_array();
  if (array == nullptr) {
    return -1;
  }

  for (bool value : list_b) {
    array->add_b(value);
  }

  AICPUE_LOGD("Finish add list bool attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddIntAttrToNodeDef(const ge::Operator &op, const char *name,
                            [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  int64_t i = 0;
  ge::graphStatus ret = op.GetAttr(name, i);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  attr_value.set_i(i);

  AICPUE_LOGD("Finish add int attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddListIntAttrToNodeDef(const ge::Operator &op, const char *name,
                                [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::vector<int64_t> list_i;
  ge::graphStatus ret = op.GetAttr(name, list_i);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto array = attr_value.mutable_array();
  if (array == nullptr) {
    return -1;
  }

  for (int64_t value : list_i) {
    array->add_i(value);
  }

  AICPUE_LOGD("Finish add list int attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddListListIntAttrToNodeDef(const ge::Operator &op, const char *name,
                                    [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::vector<std::vector<int64_t>> list_i;
  ge::graphStatus ret = op.GetAttr(name, list_i);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto array = attr_value.mutable_list_list_int();
  if (array == nullptr) {
    return -1;
  }

  array->clear_list_list_i();
  for (const std::vector<int64_t> &i : list_i) {
    const auto list_i = array->add_list_list_i();
    for (const int64_t val : i) {
      list_i->add_list_i(val);
    }
  }

  AICPUE_LOGD("Finish add list int int attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddDataTypeAttrToNodeDef(const ge::Operator &op, const char *name,
                                 [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  ge::DataType data_type = ge::DT_UNDEFINED;
  ge::graphStatus ret = op.GetAttr(name, data_type);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  attr_value.set_type(data_type);

  AICPUE_LOGD("Finish add datatype attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddListDataTypeAttrToNodeDef(const ge::Operator &op, const char *name,
                                     [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::vector<ge::DataType> list_type;
  ge::graphStatus ret = op.GetAttr(name, list_type);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto array = attr_value.mutable_array();
  if (array == nullptr) {
    return -1;
  }

  for (ge::DataType value : list_type) {
    array->add_type(value);
  }

  AICPUE_LOGD("Finish add list datatype attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddTensorAttrToNodeDef(const ge::Operator &op, const char *name,
                               [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  ge::Tensor ge_tensor;
  ge::graphStatus ret = op.GetAttr(name, ge_tensor);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto aicpu_tensor = attr_value.mutable_tensor();
  if (aicpu_tensor == nullptr) {
    return -1;
  }

  ge::TensorDesc ge_tensor_desc = ge_tensor.GetTensorDesc();
  aicpu_tensor->set_tensor_type(ge_tensor_desc.GetDataType());
  aicpu_tensor->set_data_ptr(static_cast<uint64_t>(reinterpret_cast<intptr_t>(ge_tensor.GetData())));
  aicpu_tensor->set_data_size(static_cast<uint64_t>(ge_tensor.GetSize()));
  auto shape = aicpu_tensor->mutable_tensor_shape();
  if (shape == nullptr) {
    return -1;
  }

  shape->clear_dim();
  std::vector<int64_t> dims = ge_tensor_desc.GetShape().GetDims();
  for (size_t i = 0; i < dims.size(); i++) {
    aicpuops::TensorShape_Dim *aicpu_dims = shape->add_dim();
    if (aicpu_dims == nullptr) {
      return -1; 
    }
    aicpu_dims->set_size(dims[i]);
  }

  AICPUE_LOGD("Finish add tensor attr to neod def, name[%s].", name);                           
  return 0;
}

int32_t AddListTensorAttrToNodeDef(const ge::Operator &op, const char *name,
                                   [[maybe_unused]] aicpuops::NodeDef node_def, aicpuops::AttrValue &attr_value) {
  std::vector<ge::Tensor> ge_list_tensor;
  ge::graphStatus ret = op.GetAttr(name, ge_list_tensor);
  if (ret != ge::GRAPH_SUCCESS) {
    return -1;
  }

  auto array = attr_value.mutable_array();
  if (array == nullptr) {
    return -1;
  }

  for (const ge::Tensor &ge_tensor : ge_list_tensor) {
    auto aicpu_tensor = array->add_tensor();
    if (aicpu_tensor == nullptr) {
      return -1;
    }
  
    ge::TensorDesc ge_tensor_desc = ge_tensor.GetTensorDesc();
    aicpu_tensor->set_tensor_type(ge_tensor_desc.GetDataType());
    aicpu_tensor->set_data_ptr(static_cast<uint64_t>(reinterpret_cast<intptr_t>(ge_tensor.GetData())));
    aicpu_tensor->set_data_size(static_cast<uint64_t>(ge_tensor.GetSize()));
    auto shape = aicpu_tensor->mutable_tensor_shape();
    if (shape == nullptr) {
      return -1;
    }

    shape->clear_dim();
    std::vector<int64_t> dims = ge_tensor_desc.GetShape().GetDims();
    for (size_t i = 0; i < dims.size(); i++) {
      aicpuops::TensorShape_Dim *aicpu_dims = shape->add_dim();
      if (aicpu_dims == nullptr) {
        return -1; 
      }
      aicpu_dims->set_size(dims[i]);
    }
  }

  AICPUE_LOGD("Finish add list tensor attr to neod def, name[%s].", name);
  return 0;
}

int32_t AddListAttrToNodeDef(const ge::Operator &op, const char *name,
                             const std::string &type,
                             aicpuops::NodeDef node_def,
                             aicpuops::AttrValue &attr_value) {
  int32_t ret = 0;
  if (type == kVtListString) {
    ret = AddListStringAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtListFloat) {
    ret = AddListFloatAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtListInt) {
    ret = AddListIntAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtListListInt) {
    ret = AddListListIntAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtListBool) {
    ret = AddListBoolAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtListDataType) {
    ret = AddListDataTypeAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtListTensor) {
    ret = AddListTensorAttrToNodeDef(op, name, node_def, attr_value);
  } else {
    AICPUE_LOGW("Attr type is unsuported, name: [%s], type: [%s].",
           name, type.c_str());
  }
  return ret;
}

int32_t AddAttrToNodeDef(const ge::Operator &op, const char *name,
                         const std::string type, aicpuops::NodeDef node_def,
                         aicpuops::AttrValue &attr_value) {
  int32_t ret = 0;
  if (type.empty() || type[0] == '_') {
    return ret;
  }
  if (type == kVtString) {
    ret = AddStringAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtFloat) {
    ret = AddFloatAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtInt) {
    ret = AddIntAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtBool) {
    ret = AddBoolAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtDataType) {
    ret = AddDataTypeAttrToNodeDef(op, name, node_def, attr_value);
  } else if (type == kVtTensor) {
    ret = AddTensorAttrToNodeDef(op, name, node_def, attr_value);
  } else {
    ret = AddListAttrToNodeDef(op, name, type, node_def, attr_value);
  }
  return ret;
}

std::string GetRealPath(const std::string &path) {
  char resoved_path[PATH_MAX] = {0};
  if(realpath(path.c_str(), resoved_path) != nullptr) {
    return std::string(resoved_path);
  }
  AICPUE_LOGW("path %s does not exist", path.c_str());
  return "";
}

std::string EnsureTrailingSlash(const std::string &path) {
  if (path.empty() || path.back() == kPathSeparator) {
    return path;
  }
  return path + kPathSeparator;
}

bool IsConstantFoldingSo(const std::string &file_name) {
  if (file_name.find(kConstantFoldingSoPrefix) != 0U) {
    return false;
  }
  size_t suffix_len = strlen(kConstantFoldingSoSuffix);
  if (file_name.length() < suffix_len) {
    return false;
  }
  if (file_name.compare(file_name.length() - suffix_len,
                        suffix_len, kConstantFoldingSoSuffix) != 0) {
    return false;
  }
  return file_name != kExcludedConstantFoldingSo;
}

void TryBindV2Symbols(void *handle, const std::string &so_name) {
  V2ModuleBinding binding;
  binding.handle = handle;
  binding.so_name = so_name;

  binding.get_all_op_types = reinterpret_cast<GetAllRegisteredOpTypesV2Fn>(
      dlsym(handle, kSymGetAllRegisteredOpTypesV2));
  binding.is_registered = reinterpret_cast<IsRegisteredV2Fn>(
      dlsym(handle, kSymIsRegisteredV2));
  binding.run_cpu_kernel = reinterpret_cast<RunCpuKernelV2Fn>(
      dlsym(handle, kSymRunCpuKernelV2));

  if ((binding.get_all_op_types == nullptr) &&
      (binding.run_cpu_kernel == nullptr)) {
    AICPUE_LOGW("V2 C ABI symbols not found in so[%s], skip V2 binding.",
                so_name.c_str());
    return;
  }
  g_v2_bindings.emplace_back(binding);
}

void LoadConstantFoldingSo(const std::string &base_path) {
  std::string dir_path = base_path + "opp/built-in/op_impl/host_cpu/";
  DIR *dir = opendir(dir_path.c_str());
  if (dir == nullptr) {
    AICPUE_LOGW("Failed to open directory: %s", dir_path.c_str());
    return;
  }
  struct dirent *entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    if (!IsConstantFoldingSo(entry->d_name)) {
      continue;
    }
    std::string lib_path = GetRealPath(dir_path + entry->d_name);
    if (lib_path.empty()) {
      continue;
    }
    AICPUE_LOGI("Found constant folding so: %s", lib_path.c_str());
    void *handle = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (handle == nullptr) {
      AICPUE_LOGW("dlopen failed: %s, reason: %s",
                   lib_path.c_str(), dlerror());
      continue;
    }
    AICPUE_LOGI("Successfully loaded: %s", lib_path.c_str());
    TryBindV2Symbols(handle, entry->d_name);
  }
  closedir(dir);
}
}  // namespace

void RegisterHostCpuOp(std::vector<std::string> ops, ge::HostCpuOp *(*create_fn)()) {
  std::set<std::string> black_list = {"Assign", "NoOp", "TruncatedNormal"};
  for (const std::string &op_type : ops) {
    if (black_list.find(op_type) != black_list.end()) {
      continue;
    }
    AICPUE_LOGI("Register op[%s].", op_type.c_str());
    ::ge::HostCpuOpRegistrar registrar __attribute__((unused)) =
        ::ge::HostCpuOpRegistrar(op_type.c_str(), create_fn);
  }
}

extern "C" {
__attribute__((visibility("default"))) int32_t InitCpuConstantFoldingNew(
    ge::HostCpuOp *(*create_fn)()) {
  AICPUE_LOGI("Init cpu constant folding begin.");

  const char *path_env = nullptr;
  MM_SYS_GET_ENV(MM_ENV_ASCEND_HOME_PATH, path_env);
  if (path_env == nullptr) {
    AICPUE_LOGE("ASCEND_HOME_PATH environment variable not found");
    return -1;
  }

  std::string base_path = EnsureTrailingSlash(path_env);
  AICPUE_LOGI("ASCEND_HOME_PATH is %s", base_path.c_str());

  LoadConstantFoldingSo(base_path);

  std::vector<std::string> ops =
      aicpu::CpuKernelRegister::Instance().GetAllRegisteredOpTypes();
  AICPUE_LOGI("Registered V1 ops: %llu", static_cast<uint64_t>(ops.size()));
  RegisterHostCpuOp(ops, create_fn);

  // 枚举每个ops so的V2算子, 同时构建op_type->binding反向索引。
  g_v2_op_index.clear();
  for (const auto &binding : g_v2_bindings) {
    if (binding.get_all_op_types == nullptr) {
      continue;
    }
    std::vector<std::string> ops_v2 = binding.get_all_op_types();
    for (const std::string &op_type : ops_v2) {
      // 多so重复注册同一op_type时保留先加载者, 避免后加载者静默覆盖。
      auto insert_ret = g_v2_op_index.emplace(op_type, &binding);
      if (!insert_ret.second) {
        AICPUE_LOGW(
            "op type [%s] V2 already indexed by so[%s], so[%s] skipped.",
            op_type.c_str(), insert_ret.first->second->so_name.c_str(),
            binding.so_name.c_str());
      }
    }
    AICPUE_LOGI("Registered V2 ops from so[%s]: %llu",
                binding.so_name.c_str(),
                static_cast<uint64_t>(ops_v2.size()));
    RegisterHostCpuOp(ops_v2, create_fn);
  }

  return 0;
}
int32_t BuildInputTensors(const ge::OpDescPtr &op_desc,
    const std::map<std::string, const ge::Tensor> &inputs,
    const char *op_type, aicpuops::NodeDef &node_def) {
  uint32_t count = static_cast<uint32_t>(op_desc->GetAllInputsSize());
  for (uint32_t i = 0; i < count; ++i) {
    ge::GeTensorDescPtr desc = op_desc->MutableInputDesc(i);
    if (desc == nullptr) {
      continue;
    }
    std::string name = op_desc->GetInputNameByIndex(i);
    auto iter = inputs.find(name);
    if (iter == inputs.end()) {
      AICPUE_LOGW("Op[%s] input[%s] not found.", op_type, name.c_str());
      return -1;
    }
    aicpuops::Tensor *tensor = node_def.add_inputs();
    if (tensor == nullptr) {
      return -1;
    }
    ConvertGeToAicpuTensor(*desc, name, iter->second, tensor);
  }
  return 0;
}

int32_t BuildOutputTensors(const ge::OpDescPtr &op_desc,
    std::map<std::string, ge::Tensor> &outputs,
    const char *op_type, aicpuops::NodeDef &node_def) {
  uint32_t count = static_cast<uint32_t>(op_desc->GetOutputsSize());
  for (uint32_t i = 0; i < count; ++i) {
    ge::GeTensorDesc desc = op_desc->GetOutputDesc(i);
    std::string name = op_desc->GetOutputNameByIndex(i);
    auto iter = outputs.find(name);
    if (iter == outputs.end()) {
      AICPUE_LOGW("Op[%s] output[%s] not found.", op_type, name.c_str());
      return -1;
    }
    aicpuops::Tensor *tensor = node_def.add_outputs();
    if (tensor == nullptr) {
      return -1;
    }
    ConvertGeToAicpuTensor(desc, name, iter->second, tensor);
  }
  return 0;
}

int32_t BuildNodeDefAttrs(const ge::Operator &op,
                          aicpuops::NodeDef &node_def) {
  std::map<ge::AscendString, ge::AscendString> attrs;
  if (op.GetAllAttrNamesAndTypes(attrs) != ge::GRAPH_SUCCESS) {
    return -1;
  }
  for (const auto &attr : attrs) {
    const char *name = attr.first.GetString();
    std::string type = std::string(attr.second.GetString());
    aicpuops::AttrValue attr_value;
    int32_t ret = AddAttrToNodeDef(op, name, type, node_def, attr_value);
    if (ret != 0) {
      return ret;
    }
    auto *node_def_attrs = node_def.mutable_attrs();
    if (node_def_attrs == nullptr) {
      return -1;
    }
    auto pair = node_def_attrs->insert(
        AttrValueMap::value_type(std::string(name), attr_value));
    if (!pair.second) {
      return -1;
    }
  }
  return 0;
}

int32_t BuildNodeDef(const ge::Operator &op, const std::string &op_type_str,
    const std::map<std::string, const ge::Tensor> &inputs,
    std::map<std::string, ge::Tensor> &outputs,
    aicpuops::NodeDef &node_def) {
  auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
  if (op_desc == nullptr) {
    AICPUE_LOGW("Op[%s] get op desc failed.", op_type_str.c_str());
    return -1;
  }
  node_def.set_op(op_type_str);
  int32_t ret = BuildInputTensors(
      op_desc, inputs, op_type_str.c_str(), node_def);
  if (ret != 0) {
    return ret;
  }
  ret = BuildOutputTensors(
      op_desc, outputs, op_type_str.c_str(), node_def);
  if (ret != 0) {
    return ret;
  }
  return BuildNodeDefAttrs(op, node_def);
}

// 查找op_type对应的V2 binding。未命中返回nullptr表示走V1路径。
const V2ModuleBinding *LookupV2Binding(const std::string &op_type) {
  auto iter = g_v2_op_index.find(op_type);
  if (iter == g_v2_op_index.end()) {
    return nullptr;
  }
  const V2ModuleBinding *binding = iter->second;
  if ((binding == nullptr) || (binding->run_cpu_kernel == nullptr)) {
    return nullptr;
  }
  return binding;
}

__attribute__((visibility("default"))) int32_t CpuConstantFoldingComputeNew(
    const ge::Operator &op,
    const std::map<std::string, const ge::Tensor> &inputs,
    std::map<std::string, ge::Tensor> outputs) {
  ge::AscendString op_type;
  if (op.GetOpType(op_type) != ge::GRAPH_SUCCESS) {
    return -1;
  }
  AICPUE_LOGI("Enter cpu op[%s].", op_type.GetString());
  std::string op_type_str(op_type.GetString());

  const V2ModuleBinding *hit_binding = LookupV2Binding(op_type_str);
  if (hit_binding == nullptr) {
    if (aicpu::CpuKernelRegister::Instance().GetCpuKernel(op_type_str) == nullptr) {
      AICPUE_LOGW("op type [%s] is not registered in v1 nor v2.",
                  op_type.GetString());
      return -1;
    }
    AICPUE_LOGI("op type [%s] use v1 kernel from local register.",
                op_type.GetString());
  } else {
    AICPUE_LOGI("op type [%s] hit v2 in so[%s].",
                op_type.GetString(), hit_binding->so_name.c_str());
  }

  aicpuops::NodeDef node_def;
  int32_t ret = BuildNodeDef(op, op_type_str, inputs, outputs, node_def);
  if (ret != 0) {
    return ret;
  }

  aicpu::CpuKernelContext ctx(aicpu::HOST);
  ret = ctx.Init(&node_def);
  if (ret != 0) {
    return -1;
  }

  if (hit_binding != nullptr) {
    AICPUE_LOGI("op type [%s] run cpu kernel v2 in so[%s].",
                op_type.GetString(), hit_binding->so_name.c_str());
    ret = static_cast<int32_t>(hit_binding->run_cpu_kernel(ctx));
  } else {
    AICPUE_LOGI("op type [%s] run cpu kernel v1.", op_type.GetString());
    ret = static_cast<int32_t>(
        aicpu::CpuKernelRegister::Instance().RunCpuKernel(ctx));
  }
  if (ret != 0) {
    return -1;
  }

  AICPUE_LOGI("Finish cpu op[%s].", op_type.GetString());
  return 0;
}
}
