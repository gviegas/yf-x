//
// CG
// PassVK.h
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#ifndef YF_CG_PASSVK_H
#define YF_CG_PASSVK_H

#include <utility>
#include <stdexcept>

#include "Defs.h"
#include "Pass.h"
#include "VK.h"
#include "ImageVK.h"

CG_NS_BEGIN

class TargetVK;

class PassVK final : public Pass {
 public:
  PassVK(const std::vector<AttachDesc>* colors,
         const std::vector<AttachDesc>* resolves,
         const AttachDesc* depthStencil);

  ~PassVK();

  Target::Ptr target(Size2 size, uint32_t layers,
                     const std::vector<AttachImg>* colors,
                     const std::vector<AttachImg>* resolves,
                     const AttachImg* depthStencil);

  const std::vector<AttachDesc>* colors() const;
  const std::vector<AttachDesc>* resolves() const;
  const AttachDesc* depthStencil() const;

  /// Load/Store operations for an attachment.
  ///
  using LoadStoreOp = std::pair<VkAttachmentLoadOp, VkAttachmentStoreOp>;

  /// Getter.
  ///
  VkRenderPass renderPass();

 private:
  std::vector<AttachDesc>* colors_{};
  std::vector<AttachDesc>* resolves_{};
  AttachDesc* depthStencil_{};
  VkRenderPass renderPass_ = VK_NULL_HANDLE;

  void setColors(std::vector<VkAttachmentDescription>&,
                 std::vector<VkAttachmentReference>&,
                 const std::vector<LoadStoreOp>&);

  void setDepthStencil(std::vector<VkAttachmentDescription>&,
                       std::vector<VkAttachmentReference>&,
                       LoadStoreOp, LoadStoreOp);

  void setSubpass(VkSubpassDescription&,
                  const std::vector<VkAttachmentReference>&);

  VkRenderPass createRenderPass(const std::vector<VkAttachmentDescription>&,
                                const VkSubpassDescription&);
};

class TargetVK final : public Target {
 public:
  TargetVK(PassVK& pass, Size2 size, uint32_t layers,
           const std::vector<AttachImg>* colors,
           const std::vector<AttachImg>* resolves,
           const AttachImg* depthStencil);

  ~TargetVK();

  Pass& pass();
  Size2 size() const;
  uint32_t layers() const;
  const std::vector<AttachImg>* colors() const;
  const std::vector<AttachImg>* resolves() const;
  const AttachImg* depthStencil() const;

  /// Getter.
  ///
  VkFramebuffer framebuffer();

 private:
  PassVK& pass_;
  const Size2 size_{0};
  const uint32_t layers_{};
  std::vector<AttachImg>* colors_{};
  std::vector<AttachImg>* resolves_{};
  AttachImg* depthStencil_{};
  VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
  std::vector<ImageVK::View::Ptr> views_{};
};

CG_NS_END

#endif // YF_CG_PASSVK_H
