//
// CG
// PassVK.cxx
//
// Copyright © 2020-2023 Gustavo C. Viegas.
//

#include <cassert>

#include "PassVK.h"
#include "ImageVK.h"
#include "DeviceVK.h"
#include "yf/Except.h"

using namespace CG_NS;
using namespace std;

//
// PassVK
//

PassVK::PassVK(const vector<AttachDesc>* colors,
               const vector<AttachDesc>* resolves,
               const AttachDesc* depthStencil) {

  const auto& lim = deviceVK().physLimits();
  if (colors && colors->size() > lim.maxColorAttachments)
    throw invalid_argument("Too many color attachments");

  try {
    if (colors) {
      for (const auto& color : *colors) {
        if (aspectOfVK(color.format) != VK_IMAGE_ASPECT_COLOR_BIT)
          throw invalid_argument("Invalid format for color attachment");
      }
      colors_ = new auto(*colors);

      if (resolves) {
        if (colors->size() != resolves->size())
          throw invalid_argument("Pass color/resolve attachment mismatch");

        for (size_t i = 0; i < colors->size(); i++) {
          if ((*colors)[i].format != (*resolves)[i].format)
            throw invalid_argument("Pass color/resolve attachment mismatch");
        }
        resolves_ = new auto(*resolves);
      }
    } else if (resolves) {
      throw invalid_argument("Pass color/resolve attachment mismatch");
    }

    if (depthStencil) {
      auto aspect = aspectOfVK(depthStencil->format);
      if (!(aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)))
        throw invalid_argument("Invalid format for depth/stencil attachment");

      depthStencil_ = new auto(*depthStencil);
    }
  } catch (const bad_alloc&) {
    delete colors_;
    delete resolves_;
    delete depthStencil_;
    throw;
  }
}

PassVK::~PassVK() {
  delete colors_;
  delete resolves_;
  delete depthStencil_;

  // TODO: Notify
  auto dev = deviceVK().device();
  for (auto& rp : renderPasses_)
    vkDestroyRenderPass(dev, rp.renderPass, nullptr);
}

void PassVK::setColors(vector<VkAttachmentDescription>& descs,
                       vector<VkAttachmentReference>& refs,
                       const vector<LoadStoreOp>& ops) {
  auto op = ops.begin();

  for (const auto& color : *colors_) {
    descs.push_back({});
    auto& desc = descs.back();
    desc.flags = 0;
    desc.format = toFormatVK(color.format);
    desc.samples = toSingleSampleCountVK(color.samples);
    desc.loadOp = toLoadOpVK(op->first);
    desc.storeOp = toStoreOpVK(op->second);
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    if (op->first != LoadOpLoad)
      desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    else
      desc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    desc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    refs.push_back({
      static_cast<uint32_t>(descs.size() - 1),
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    op++;
  }
}

void PassVK::setDepthStencil(vector<VkAttachmentDescription>& descs,
                             vector<VkAttachmentReference>& refs,
                             LoadStoreOp depthOp, LoadStoreOp stencilOp) {
  descs.push_back({});
  auto& desc = descs.back();
  desc.flags = 0;
  desc.format = toFormatVK(depthStencil_->format);
  desc.samples = toSingleSampleCountVK(depthStencil_->samples);
  desc.loadOp = toLoadOpVK(depthOp.first);
  desc.storeOp = toStoreOpVK(depthOp.second);
  desc.stencilLoadOp = toLoadOpVK(stencilOp.first);
  desc.stencilStoreOp = toStoreOpVK(stencilOp.second);
  if (depthOp.first != LoadOpLoad && stencilOp.first != LoadOpLoad)
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  else
    desc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
  desc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

  refs.push_back({
    static_cast<uint32_t>(descs.size() - 1),
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
  });
}

void PassVK::setSubpass(VkSubpassDescription& subpass,
                        const vector<VkAttachmentReference>& refs) {
  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = nullptr;
  if (colors_) {
    subpass.colorAttachmentCount = colors_->size();
    subpass.pColorAttachments = refs.data();
  } else {
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments = nullptr;
  }
  subpass.pResolveAttachments = nullptr;
  if (depthStencil_)
    subpass.pDepthStencilAttachment = &refs.data()[refs.size() - 1];
  else
    subpass.pDepthStencilAttachment = nullptr;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = nullptr;
}

VkRenderPass PassVK::createRenderPass(
  const vector<VkAttachmentDescription>& descs,
  const VkSubpassDescription& subpass) {

  VkRenderPassCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.attachmentCount = descs.size();
  info.pAttachments = descs.data();
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 0;
  info.pDependencies = nullptr;

  VkRenderPass renderPass;
  VkResult res = vkCreateRenderPass(deviceVK().device(), &info, nullptr,
                                    &renderPass);
  if (res != VK_SUCCESS)
    throw DeviceExcept("Could not create render pass");
  return renderPass;
}

Target::Ptr PassVK::target(Size2 size, uint32_t layers,
                           const vector<AttachImg>* colors,
                           const vector<AttachImg>* resolves,
                           const AttachImg* depthStencil) {

  return make_unique<TargetVK>(*this, size, layers, colors, resolves,
                               depthStencil);
}

const vector<AttachDesc>* PassVK::colors() const {
  return colors_;
}

const vector<AttachDesc>* PassVK::resolves() const {
  return resolves_;
}

const AttachDesc* PassVK::depthStencil() const {
  return depthStencil_;
}

VkRenderPass PassVK::renderPass() {
  const LoadStoreOp op{LoadOpLoad,/*Clear,*/ StoreOpStore};
  if (colors_)
    return renderPass(vector<LoadStoreOp>(colors_->size(), op), op, op);
  return renderPass({}, op, op);
}

VkRenderPass PassVK::renderPass(const vector<LoadStoreOp>& colors,
                                LoadStoreOp depth, LoadStoreOp stencil) {
  RenderPass* renderPass = &renderPasses_[0];
  for (auto& rp : renderPasses_) {
    if (rp.renderPass == VK_NULL_HANDLE) {
      renderPass = &rp;
      break;
    }
    if (rp.equalOp(colors, depth, stencil))
      return rp.renderPass;
  }

  // Need a new render pass for these operations
  vector<VkAttachmentDescription> descs;
  vector<VkAttachmentReference> refs;
  VkSubpassDescription subpass;
  if (colors_) {
    assert(colors.size() == colors_->size());
    setColors(descs, refs, colors);
  }
  if (depthStencil_)
    setDepthStencil(descs, refs, depth, stencil);
  setSubpass(subpass, refs);

  vkDestroyRenderPass(deviceVK().device(), renderPass->renderPass, nullptr);
  renderPass->renderPass = VK_NULL_HANDLE;
  renderPass->renderPass = createRenderPass(descs, subpass);
  renderPass->colors = colors;
  renderPass->depth = depth;
  renderPass->stencil = stencil;
  return renderPass->renderPass;
}

//
// TargetVK
//

TargetVK::TargetVK(PassVK& pass, Size2 size, uint32_t layers,
                   const vector<AttachImg>* colors,
                   const vector<AttachImg>* resolves,
                   const AttachImg* depthStencil)
  : pass_(pass), size_(size), layers_(layers) {

  if (size.width == 0 || size.height == 0 || layers == 0)
    throw invalid_argument("TargetVK requires size > 0 and layers > 0");

  const auto& lim = deviceVK().physLimits();
  if (size.width > lim.maxFramebufferWidth ||
      size.height > lim.maxFramebufferHeight ||
      layers > lim.maxFramebufferLayers)
    throw invalid_argument("TargetVK limit");

  try {
    vector<VkImageView> handles;

    if (colors) {
      if (!pass.colors() || pass.colors()->size() != colors->size())
        throw invalid_argument("Target not compatible with pass");

      colors_ = new auto(*colors);
      createColorViews(handles);

      if (resolves) {
        if (!pass.resolves() || pass.resolves()->size() != resolves->size())
          throw invalid_argument("Target not compatible with pass");

        resolves_ = new auto(*resolves);
        // MS resolve is done outside of render pass
      }
    } else if (pass.colors()) {
      throw invalid_argument("Target not compatible with pass");
    }

    if (depthStencil) {
      if (!pass.depthStencil())
        throw invalid_argument("Target not compatible with pass");

      depthStencil_ = new auto(*depthStencil);
      createDepthStencilView(handles);

    } else if (pass.depthStencil()) {
      throw invalid_argument("Target not compatible with pass");
    }

    createFramebuffer(handles);
  } catch (...) {
    delete colors_;
    delete resolves_;
    delete depthStencil_;
    throw;
  }
}

TargetVK::~TargetVK() {
  delete colors_;
  delete resolves_;
  delete depthStencil_;

  // TODO: Notify
  auto dev = deviceVK().device();
  vkDestroyFramebuffer(dev, framebuffer_, nullptr);
}

void TargetVK::createColorViews(vector<VkImageView>& handles) {
  assert(views_.size() == 0);
  assert(handles.size() == 0);

  for (auto& color : *colors_) {
    views_.push_back(color.image->view({
      {color.level, color.level + 1},
      {color.layer, color.layer + layers_},
      layers_ == 1 ? ImgView::Dim2 : ImgView::Dim2Array}));

    handles.push_back(static_cast<ImgViewVK*>(views_.back().get())->handle());
  }
}

void TargetVK::createDepthStencilView(vector<VkImageView>& handles) {
  assert(colors_ ? views_.size() == colors_->size() : views_.empty());
  assert(colors_ ? handles.size() == colors_->size() : handles.empty());

  views_.push_back(depthStencil_->image->view({
    {depthStencil_->level, 1},
    {depthStencil_->layer, depthStencil_->layer + layers_},
    layers_ == 1 ? ImgView::Dim2 : ImgView::Dim2Array}));

  handles.push_back(static_cast<ImgViewVK*>(views_.back().get())->handle());
}

void TargetVK::createFramebuffer(const vector<VkImageView>& handles) {
  VkFramebufferCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.renderPass = pass_.renderPass();
  info.attachmentCount = handles.size();
  info.pAttachments = handles.data();
  info.width = size_.width;
  info.height = size_.height;
  info.layers = layers_;

  VkResult res = vkCreateFramebuffer(deviceVK().device(), &info, nullptr,
                                     &framebuffer_);
  if (res != VK_SUCCESS)
    throw DeviceExcept("Could not create framebuffer");
}

Pass& TargetVK::pass() {
  return pass_;
}

Size2 TargetVK::size() const {
  return size_;
}

uint32_t TargetVK::layers() const {
  return layers_;
}

const vector<AttachImg>* TargetVK::colors() const {
  return colors_;
}

const vector<AttachImg>* TargetVK::resolves() const {
  return resolves_;
}

const AttachImg* TargetVK::depthStencil() const {
  return depthStencil_;
}

VkFramebuffer TargetVK::framebuffer() {
  return framebuffer_;
}

void TargetVK::setBeginInfo(VkRenderPassBeginInfo& info,
                            vector<VkClearValue>& clearValues,
                            const TargetOp& targetOp) {
  assert(clearValues.size() == 0);
  assert(!colors_ || colors_->size() == targetOp.colorOps.size());

  const auto& colors = targetOp.colorOps;
  const auto& depth = targetOp.depthOp;
  const auto& stencil = targetOp.stencilOp;
  auto renderPass = pass_.renderPass(colors, depth, stencil);

  if (colors_) {
    auto colorValue = targetOp.colorValues.begin();
    for (const auto& color : colors) {
      VkClearValue value;
      if (color.first == LoadOpClear) {
        // TODO: SINT/UINT formats
        value.color.float32[0] = (*colorValue)[0];
        value.color.float32[1] = (*colorValue)[1];
        value.color.float32[2] = (*colorValue)[2];
        value.color.float32[3] = (*colorValue)[3];
        colorValue++;
      }
      clearValues.push_back(value);
    }
  }
  if (depthStencil_) {
    if (depth.first == LoadOpClear || stencil.first == LoadOpClear) {
      VkClearValue value;
      value.depthStencil.depth = targetOp.depthValue;
      value.depthStencil.stencil = targetOp.stencilValue;
      clearValues.push_back(value);
    }
  }

  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  info.pNext = nullptr;
  info.renderPass = renderPass;
  info.framebuffer = framebuffer_;
  info.renderArea = {{0, 0}, size_.width, size_.height};
  info.clearValueCount = clearValues.size();
  info.pClearValues = clearValues.data();
}
