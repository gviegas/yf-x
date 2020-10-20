//
// cg
// DeviceVK.h
//
// Copyright © 2020 Gustavo C. Viegas.
//

#ifndef YF_CG_DEVICEVK_H
#define YF_CG_DEVICEVK_H

#include "yf/cg/Defs.h"
#include "yf/cg/Device.h"
#include "VK.h"

CG_NS_BEGIN

class QueueVK;

class DeviceVK final : public Device {
 public:
  ~DeviceVK();

  static DeviceVK& get();

  Buffer::Ptr makeBuffer(uint64_t size);

  Image::Ptr makeImage(PxFormat format,
                       Size2 size,
                       uint32_t layers,
                       uint32_t levels,
                       Samples samples);

  Shader::Ptr makeShader(Stage stage,
                         std::wstring&& codeFile,
                         std::wstring&& entryPoint);

  DcTable::Ptr makeDcTable(const DcEntries& entries);
  DcTable::Ptr makeDcTable(DcEntries&& entries);

  Pass::Ptr makePass(const std::vector<ColorAttach>* colors,
                     const std::vector<ColorAttach>* resolves,
                     const DepStenAttach* depthStencil);

  GrState::Ptr makeState(const GrState::Config& config);
  GrState::Ptr makeState(GrState::Config&& config);
  CpState::Ptr makeState(const CpState::Config& config);
  CpState::Ptr makeState(CpState::Config&& config);

  Queue& defaultQueue();
  Queue& queue(Queue::CapabilityMask capabilities);

  /// Getters.
  ///
  VkInstance instance() const;
  VkPhysicalDevice physicalDev() const;
  VkDevice device() const;
  const VkPhysicalDeviceProperties& physProperties() const;
  const std::vector<const char*>& instExtensions() const;
  const std::vector<const char*>& devExtensions() const;
  const std::vector<const char*>& layers() const;
  uint32_t instVersion() const;
  uint32_t devVersion() const;
  const VkPhysicalDeviceLimits& limits() const;

 private:
  DeviceVK();

  Result checkInstanceExtensions();
  Result checkDeviceExtensions();
  void initInstance();
  void initPhysicalDevice();
  void initDevice(int32_t);

  VkInstance _instance = nullptr;
  uint32_t _instVersion = 0;
  std::vector<const char*> _instExtensions{};
  std::vector<const char*> _layers{};

  VkPhysicalDevice _physicalDev = nullptr;
  VkPhysicalDeviceProperties _physProperties{};

  VkDevice _device = nullptr;
  std::vector<const char*> _devExtensions{};

  QueueVK* _queue = nullptr;
};

CG_NS_END

#endif // YF_CG_DEVICEVK_H
