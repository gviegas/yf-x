//
// CG
// ShaderVK.cxx
//
// Copyright © 2020-2023 Gustavo C. Viegas.
//

#include <cwchar>
#include <cstring>
#include <memory>
#include <fstream>

#include "ShaderVK.h"
#include "DeviceVK.h"
#include "yf/Except.h"

using namespace CG_NS;
using namespace std;

ShaderVK::ShaderVK(const Desc& desc)
  : stage_(desc.stage), entryPoint_(desc.entryPoint) {

  if (desc.codeFile.empty() || desc.entryPoint.empty())
    throw invalid_argument("ShaderVK requires valid codeFile and entryPoint");

  // Get shader code data and create module
  ifstream ifs(desc.codeFile);
  if (!ifs)
    throw FileExcept("Could not open shader file");

  ifs.seekg(0, ios_base::end);
  const auto sz = ifs.tellg();
  if (sz == 0 || sz%4 != 0)
    throw FileExcept("Invalid shader file");

  ifs.seekg(0);
  auto buf = make_unique<char[]>(sz);
  if (!ifs.read(buf.get(), sz))
    throw FileExcept("Could not read data from shader file");

  VkShaderModuleCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.codeSize = sz;
  info.pCode = reinterpret_cast<uint32_t*>(buf.get());

  auto dev = deviceVK().device();
  auto res = vkCreateShaderModule(dev, &info, nullptr, &module_);
  if (res != VK_SUCCESS)
    throw DeviceExcept("Could not create shader module");
}

ShaderVK::~ShaderVK() {
  // TODO: Notify
  auto dev = deviceVK().device();
  vkDestroyShaderModule(dev, module_, nullptr);
}

Stage ShaderVK::stage() const {
  return stage_;
}

const std::string& ShaderVK::entryPoint() const {
  return entryPoint_;
}

VkShaderModule ShaderVK::module() {
  return module_;
}
