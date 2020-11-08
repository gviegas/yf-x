//
// cg
// PassVK.cxx
//
// Copyright © 2020 Gustavo C. Viegas.
//

#include <stdexcept>

#include "PassVK.h"
#include "DeviceVK.h"

using namespace CG_NS;
using namespace std;

INTERNAL_NS_BEGIN

// TODO: move these functions

inline VkFormat toFormatVK(PxFormat pxFormat) {
  switch (pxFormat) {
  case PxFormatUndefined:  return VK_FORMAT_UNDEFINED;
  case PxFormatBgra8Srgb:  return VK_FORMAT_B8G8R8A8_SRGB;
  case PxFormatRgba8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
  case PxFormatD16Unorm: return VK_FORMAT_D16_UNORM;
  }
}

inline VkSampleCountFlagBits toSampleCountVK(Samples samples) {
  switch (samples) {
  case Samples1:  return VK_SAMPLE_COUNT_1_BIT;
  case Samples2:  return VK_SAMPLE_COUNT_2_BIT;
  case Samples4:  return VK_SAMPLE_COUNT_4_BIT;
  case Samples8:  return VK_SAMPLE_COUNT_8_BIT;
  case Samples16: return VK_SAMPLE_COUNT_16_BIT;
  case Samples32: return VK_SAMPLE_COUNT_32_BIT;
  case Samples64: return VK_SAMPLE_COUNT_64_BIT;
  }
}

inline VkAttachmentLoadOp toLoadOpVK(LoadOp op) {
  switch (op) {
  case LoadOpLoad:     return VK_ATTACHMENT_LOAD_OP_LOAD;
  case LoadOpClear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
  case LoadOpDontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }
}

inline VkAttachmentStoreOp toStoreOpVK(StoreOp op) {
  switch (op) {
  case StoreOpStore:    return VK_ATTACHMENT_STORE_OP_STORE;
  case StoreOpDontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
}

INTERNAL_NS_END

// ------------------------------------------------------------------------
// PassVK

PassVK::PassVK(const vector<ColorAttach>* colors,
               const vector<ColorAttach>* resolves,
               const DepStenAttach* depthStencil)
               : Pass(colors, resolves, depthStencil) {

  vector<VkAttachmentDescription> attachDescs;
  vector<VkAttachmentReference> attachRefs;

  // Define attachments
  auto add = [&](const vector<ColorAttach>& attachs) {
    // TODO: validate parameters
    for (const auto& attach : attachs) {
      attachDescs.push_back({});
      auto desc = attachDescs.back();
      desc.flags = 0;
      desc.format = toFormatVK(attach.format);
      desc.samples = toSampleCountVK(attach.samples);
      desc.loadOp = toLoadOpVK(attach.loadOp);
      desc.storeOp = toStoreOpVK(attach.storeOp);
      desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      attachRefs.push_back({static_cast<uint32_t>(attachDescs.size()-1),
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    }
  };

  if (colors)
    add(*colors);

  if (resolves) {
    if (!colors || colors->size() != resolves->size())
      // TODO
      throw invalid_argument("Pass color/resolve attachment mismatch");
    add(*resolves);
  }

  if (depthStencil) {
    // TODO: validate parameters
    attachDescs.push_back({});
    auto desc = attachDescs.back();
    desc.flags = 0;
    desc.format = toFormatVK(depthStencil->format);
    desc.samples = toSampleCountVK(depthStencil->samples);
    desc.loadOp = toLoadOpVK(depthStencil->depLoadOp);
    desc.storeOp = toStoreOpVK(depthStencil->depStoreOp);
    desc.stencilLoadOp = toLoadOpVK(depthStencil->stenLoadOp);
    desc.stencilStoreOp = toStoreOpVK(depthStencil->stenStoreOp);
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // TODO: check format aspect and set layout accordingly
    desc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachRefs.push_back({static_cast<uint32_t>(attachDescs.size()-1),
                          VK_IMAGE_LAYOUT_GENERAL});
  }

  // Define subpass
  VkSubpassDescription subDesc;
  subDesc.flags = 0;
  subDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subDesc.inputAttachmentCount = 0;
  subDesc.pInputAttachments = nullptr;
  subDesc.preserveAttachmentCount = 0;
  subDesc.pPreserveAttachments = nullptr;

  if (colors) {
    subDesc.colorAttachmentCount = colors->size();
    subDesc.pColorAttachments = attachRefs.data();
    if (resolves)
      subDesc.pResolveAttachments = attachRefs.data()+colors->size();
    else
      subDesc.pResolveAttachments = nullptr;
  } else {
    subDesc.colorAttachmentCount = 0;
    subDesc.pColorAttachments = nullptr;
    subDesc.pResolveAttachments = nullptr;
  }

  if (depthStencil)
    subDesc.pDepthStencilAttachment = attachRefs.data()+attachRefs.size()-1;
  else
    subDesc.pDepthStencilAttachment = nullptr;

  // Create render pass
  VkRenderPassCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.attachmentCount = attachDescs.size();
  info.pAttachments = attachDescs.data();
  info.subpassCount = 1;
  info.pSubpasses = &subDesc;
  info.dependencyCount = 0;
  info.pDependencies = nullptr;

  auto dev = DeviceVK::get().device();
  auto res = vkCreateRenderPass(dev, &info, nullptr, &_renderPass);
  if (res != VK_SUCCESS)
    // TODO
    throw runtime_error("Could not create render pass");
}

PassVK::~PassVK() {
  // TODO: notify
  auto dev = DeviceVK::get().device();
  vkDestroyRenderPass(dev, _renderPass, nullptr);
}

Target::Ptr PassVK::makeTarget(Size2 size,
                               uint32_t layers,
                               const vector<AttachImg>* colors,
                               const vector<AttachImg>* resolves,
                               const AttachImg* depthStencil) {

  return make_unique<TargetVK>(*this, size, layers, colors, resolves,
                               depthStencil);
}

VkRenderPass PassVK::renderPass() const {
  return _renderPass;
}

// ------------------------------------------------------------------------
// TargetVK

TargetVK::TargetVK(PassVK& pass,
                   Size2 size,
                   uint32_t layers,
                   const vector<AttachImg>* colors,
                   const vector<AttachImg>* resolves,
                   const AttachImg* depthStencil)
                   : _pass(pass) {

  // TODO
}

TargetVK::~TargetVK() {
  // TODO: notify
  auto dev = DeviceVK::get().device();
  vkDestroyFramebuffer(dev, _framebuffer, nullptr);
}

const Pass& TargetVK::pass() const {
  return _pass;
}

VkFramebuffer TargetVK::framebuffer() const {
  return _framebuffer;
}