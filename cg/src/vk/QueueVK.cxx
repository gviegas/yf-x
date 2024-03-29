//
// CG
// QueueVK.cxx
//
// Copyright © 2020-2023 Gustavo C. Viegas.
//

#include <cstdlib>
#include <stdexcept>
#include <cassert>

#include "QueueVK.h"
#include "DeviceVK.h"
#include "BufferVK.h"
#include "DcTableVK.h"
#include "PassVK.h"
#include "StateVK.h"
#include "Cmd.h"
#include "Encoder.h"
#include "yf/Except.h"

using namespace CG_NS;
using namespace std;

//
// QueueVK
//

QueueVK::QueueVK(VkQueue handle, int32_t family)
  : handle_(handle), family_(family) {

  assert(handle != nullptr);
  assert(family > -1);

  // XXX: `DeviceVK` not fully constructed yet
}

QueueVK::~QueueVK() {
  deinitPool(poolPrio_);
  if (!pools_.empty()) {
    // XXX: No command buffer shall outlive its queue
    assert(false);
    abort();
  }
}

VkCommandPool QueueVK::initPool() {
  VkCommandPoolCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  info.queueFamilyIndex = family_;

  VkCommandPool handle;
  auto res = vkCreateCommandPool(deviceVK().device(), &info, nullptr,
                                 &handle);
  if (res != VK_SUCCESS)
    throw DeviceExcept("Could not create command pool");

  return handle;
}

void QueueVK::deinitPool(VkCommandPool pool) {
  vkDestroyCommandPool(deviceVK().device(), pool, nullptr);
}

CmdBuffer::Ptr QueueVK::cmdBuffer() {
  auto pool = initPool();

  VkCommandBufferAllocateInfo info;
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext = nullptr;
  info.commandPool = pool;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  info.commandBufferCount = 1;

  VkCommandBuffer handle;
  auto res = vkAllocateCommandBuffers(deviceVK().device(), &info, &handle);
  if (res != VK_SUCCESS) {
    deinitPool(pool);
    throw DeviceExcept("Could not allocate command buffer");
  }

  auto it = pools_.emplace(new CmdBufferVK(*this, handle), pool).first;
  return CmdBuffer::Ptr(it->first);
}

void QueueVK::submit() {
  auto dev = deviceVK().device();
  VkSemaphore sem = VK_NULL_HANDLE;
  VkResult res;

  auto notifyAndClear = [&](bool result) {
    semaphores_.clear();
    stageMasks_.clear();
    maskPrio_ = 0;
    vkDestroySemaphore(dev, sem, nullptr);

    for (auto& fn : callbsPrio_)
      fn(result);
    callbsPrio_.clear();
    pendPrio_ = false;

    for (auto& cb : pending_)
      cb->didExecute();
    pending_.clear();
  };

  if (pendPrio_) {
    res = vkEndCommandBuffer(cmdPrio_);
    if (res != VK_SUCCESS) {
      notifyAndClear(false);
      throw DeviceExcept("Could not end priority command buffer");
    }
  } else if (pending_.empty()) {
    // Nothing to do
    return;
  }

  // Set submission info
  VkSubmitInfo infos[2];
  uint32_t infoN = 0;
  vector<VkCommandBuffer> handles;

  if (pendPrio_) {
    infos[infoN].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    infos[infoN].pNext = nullptr;
    infos[infoN].waitSemaphoreCount = 0;
    infos[infoN].pWaitSemaphores = nullptr;
    infos[infoN].pWaitDstStageMask = nullptr;
    infos[infoN].commandBufferCount = 1;
    infos[infoN].pCommandBuffers = &cmdPrio_;
    infos[infoN].signalSemaphoreCount = 0;
    infos[infoN].pSignalSemaphores = nullptr;

    infoN++;
  }

  if (!pending_.empty()) {
    for (const auto& cb : pending_)
      handles.push_back(cb->handle());

    infos[infoN].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    infos[infoN].pNext = nullptr;
    infos[infoN].waitSemaphoreCount = 0;
    infos[infoN].pWaitSemaphores = nullptr;
    infos[infoN].pWaitDstStageMask = nullptr;
    infos[infoN].commandBufferCount = handles.size();
    infos[infoN].pCommandBuffers = handles.data();
    infos[infoN].signalSemaphoreCount = 0;
    infos[infoN].pSignalSemaphores = nullptr;

    infoN++;
  }

  // Sync. setup
  if (infoN == 2) {
    VkSemaphoreCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;

    res = vkCreateSemaphore(dev, &info, nullptr, &sem);
    if (res != VK_SUCCESS) {
      notifyAndClear(false);
      throw DeviceExcept("Could not create semaphore for queue submission");
    }

    infos[0].signalSemaphoreCount = 1;
    infos[0].pSignalSemaphores = &sem;

    infos[1].waitSemaphoreCount = 1;
    infos[1].pWaitSemaphores = &sem;
    infos[1].pWaitDstStageMask = &maskPrio_;
  }

  if (!semaphores_.empty()) {
    infos[0].waitSemaphoreCount = semaphores_.size();
    infos[0].pWaitSemaphores = semaphores_.data();
    infos[0].pWaitDstStageMask = stageMasks_.data();
  }

  // Submit and wait completion
  res = vkQueueSubmit(handle_, infoN, infos, VK_NULL_HANDLE);
  if (res != VK_SUCCESS) {
    notifyAndClear(false);
    throw DeviceExcept("Queue submission failed");
  }

  res = vkQueueWaitIdle(handle_);
  if (res != VK_SUCCESS) {
    notifyAndClear(false);
    throw DeviceExcept("Could not wait for queue operations to complete");
  }

  notifyAndClear(true);
}

Queue::CapabilityMask QueueVK::capabilities() const {
  return Graphics|Compute|Transfer;
}

void QueueVK::enqueue(CmdBufferVK* cmdBuffer) {
  assert(pending_.find(cmdBuffer) == pending_.end());

  // TODO: Lock
  pending_.insert(cmdBuffer);
}

void QueueVK::unmake(CmdBufferVK* cmdBuffer) noexcept {
  assert(pools_.find(cmdBuffer) != pools_.end());

  if (cmdBuffer->isPending()) {
    // TODO: Gate command buffer destruction
    assert(false);
    abort();
  }

  auto it = pools_.find(cmdBuffer);
  vkDestroyCommandPool(deviceVK().device(), it->second, nullptr);
  pools_.erase(it);
}

VkCommandBuffer QueueVK::getPriority(VkPipelineStageFlags stageMask,
                                     function<void (bool)> completionHandler) {

  if (stageMask == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
    maskPrio_ = stageMask;
  else
    maskPrio_ |= stageMask;

  if (pendPrio_) {
    callbsPrio_.push_back(completionHandler);
    return cmdPrio_;
  }

  VkResult res;

  if (!cmdPrio_) {
    poolPrio_ = initPool();

    VkCommandBufferAllocateInfo info;
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;
    info.commandPool = poolPrio_;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    res = vkAllocateCommandBuffers(deviceVK().device(), &info, &cmdPrio_);
    if (res != VK_SUCCESS) {
      deinitPool(poolPrio_);
      poolPrio_ = VK_NULL_HANDLE;
      throw DeviceExcept("Could not allocate command buffer");
    }
  }

  VkCommandBufferBeginInfo info;
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext = nullptr;
  info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  info.pInheritanceInfo = nullptr;

  res = vkBeginCommandBuffer(cmdPrio_, &info);
  if (res != VK_SUCCESS)
    throw DeviceExcept("Could not begin command buffer");

  callbsPrio_.push_back(completionHandler);
  pendPrio_ = true;
  return cmdPrio_;
}

void QueueVK::waitFor(VkSemaphore semaphore, VkPipelineStageFlags stageMask) {
  semaphores_.push_back(semaphore);
  stageMasks_.push_back(stageMask);
}

VkQueue QueueVK::handle() {
  return handle_;
}

int32_t QueueVK::family() const {
  return family_;
}

//
// CmdBufferVK
//

CmdBufferVK::CmdBufferVK(QueueVK& queue, VkCommandBuffer handle)
  : queue_(queue), handle_(handle), pending_(false), begun_(false) {

  assert(handle != nullptr);
}

CmdBufferVK::~CmdBufferVK() {
  queue_.unmake(this);
}

void CmdBufferVK::encode(const Encoder& encoder) {
  if (pending_)
    throw runtime_error("Attempt to encode a pending command buffer");

  if (!begun_) {
    VkCommandBufferBeginInfo info;
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;
    // TODO: Consider reusing the command buffer instead
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = nullptr;

    auto res = vkBeginCommandBuffer(handle_, &info);
    if (res != VK_SUCCESS)
      throw DeviceExcept("Could not set command buffer for encoding");

    begun_ = true;
  }

  try {
    switch (encoder.type()) {
    case Encoder::Graphics:
      encode(static_cast<const GrEncoder&>(encoder));
      break;
    case Encoder::Compute:
      encode(static_cast<const CpEncoder&>(encoder));
      break;
    case Encoder::Transfer:
      encode(static_cast<const TfEncoder&>(encoder));
      break;
    }
  } catch (...) {
    reset();
    throw;
  }
}

void CmdBufferVK::enqueue() {
  if (pending_)
    throw runtime_error("Attempt to enqueue a pending command buffer");

  if (!begun_)
    throw runtime_error("Attempt to enqueue an empty command buffer");

  begun_ = false;

  auto res = vkEndCommandBuffer(handle_);
  if (res != VK_SUCCESS)
    throw DeviceExcept("Invalid command buffer encoding(s)");

  pending_ = true;
  queue_.enqueue(this);
}

void CmdBufferVK::reset() {
  if (pending_)
    throw runtime_error("Attempt to reset a pending command buffer");

  vkResetCommandBuffer(handle_, 0);
}

bool CmdBufferVK::isPending() {
  return pending_;
}

Queue& CmdBufferVK::queue() {
  return queue_;
}

VkCommandBuffer CmdBufferVK::handle() {
  return handle_;
}

void CmdBufferVK::didExecute() {
  assert(pending_);

  pending_ = false;
}

void CmdBufferVK::encode(const GrEncoder& encoder) {
  enum : uint32_t {
    SVport = 0x01,
    SSciss = 0x02,
    STgt   = 0x04,
    SGst   = 0x08,
    SVbuf  = 0x10,
    SIbuf  = 0x20,

    SDraw  = 0x1F, // Can draw?
    SDrawi = 0x3F, // Can draw indexed?
    SNone  = 0
  };

  uint32_t status = SNone;
  TargetVK* tgt = nullptr;
  const TargetOp* tgtOp = nullptr;
  GrStateVK* gst = nullptr;
  vector<const DcTableCmd*> dtbs;

  // Begin render pass
  auto beginPass = [&] {
    VkRenderPassBeginInfo info;
    vector<VkClearValue> clear;
    tgt->setBeginInfo(info, clear, *tgtOp);

    vkCmdBeginRenderPass(handle_, &info, VK_SUBPASS_CONTENTS_INLINE);
    status |= STgt;
  };

  // End render pass
  auto endPass = [&] {
    vkCmdEndRenderPass(handle_);
    tgt = nullptr;
    status &= ~STgt;
  };

  // Bind descriptor sets
  auto bindSets = [&] {
    auto plLay = gst->plLayout();

    for (const auto& d : dtbs) {
      auto i = d->tableIndex;
      auto j = d->allocIndex;

      if (i >= gst->config().dcTables.size() ||
          j >= gst->config().dcTables[i]->allocations())
        throw invalid_argument("setDcTable() index out of range");

      auto ds = static_cast<DcTableVK*>(gst->config().dcTables[i])->ds(j);
      vkCmdBindDescriptorSets(handle_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              plLay, i, 1, &ds, 0, nullptr);
    }

    dtbs.clear();
  };

  // Set viewport
  auto setViewport = [&](const ViewportCmd* sub) {
    // TODO: Support for multiple viewports
    if (sub->viewportIndex != 0)
      throw UnsupportedExcept("Multiple viewports not supported");

    VkViewport vport{sub->viewport.x, sub->viewport.y,
                     sub->viewport.width, sub->viewport.height,
                     sub->viewport.zNear, sub->viewport.zFar};

    vkCmdSetViewport(handle_, sub->viewportIndex, 1, &vport);
    status |= SVport;
  };

  // Set scissor
  auto setScissor = [&](const ScissorCmd* sub) {
    // TODO: Support for multiple viewports
    if (sub->viewportIndex != 0)
      throw UnsupportedExcept("Multiple viewports not supported");

    VkRect2D sciss{{sub->scissor.offset.x, sub->scissor.offset.y},
                   {sub->scissor.size.width, sub->scissor.size.height}};

    vkCmdSetScissor(handle_, sub->viewportIndex, 1, &sciss);
    status |= SSciss;
  };

  // Set target
  auto setTarget = [&](const TargetCmd* sub) {
    if (tgt)
      endPass();
    tgt = &static_cast<TargetVK&>(sub->target);
    tgtOp = &sub->targetOp;
    beginPass();
  };

  // Set graphics state
  auto setState = [&](const StateGrCmd* sub) {
    auto st = &static_cast<GrStateVK&>(sub->state);
    if (st != gst) {
      gst = st;
      auto pl = gst->pipeline();
      vkCmdBindPipeline(handle_, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
      status |= SGst;
    }
  };

  // Set descriptor table
  auto setDcTable = [&](const DcTableCmd* sub) {
    dtbs.push_back(sub);
  };

  // Set vertex buffer
  auto setVxBuffer = [&](const VxBufferCmd* sub) {
    auto buf = &static_cast<BufferVK&>(sub->buffer);
    auto bufHandle = buf->handle();
    auto off = sub->offset;

    vkCmdBindVertexBuffers(handle_, sub->inputIndex, 1, &bufHandle, &off);
    status |= SVbuf;
  };

  // Set index buffer
  auto setIxBuffer = [&](const IxBufferCmd* sub) {
    auto buf = &static_cast<BufferVK&>(sub->buffer);
    auto bufHandle = buf->handle();
    auto type = sub->type == IndexTypeU16 ? VK_INDEX_TYPE_UINT16
                                          : VK_INDEX_TYPE_UINT32;

    vkCmdBindIndexBuffer(handle_, bufHandle, sub->offset, type);
    status |= SIbuf;
  };

  // Draw
  auto draw = [&](const DrawCmd* sub) {
    if ((status & SDraw) != SDraw)
      throw invalid_argument("Invalid draw() encoding");

    if (!dtbs.empty())
      bindSets();

    // TODO: Check limits
    vkCmdDraw(handle_, sub->vertexCount, sub->instanceCount,
              sub->vertexStart, sub->baseInstance);
  };

  // Draw indexed
  auto drawIx = [&](const DrawIxCmd* sub) {
    if ((status & SDrawi) != SDrawi)
      throw invalid_argument("Invalid drawIndexed() encoding");

    if (!dtbs.empty())
      bindSets();

    // TODO: Check limits
    vkCmdDrawIndexed(handle_, sub->vertexCount, sub->instanceCount,
                     sub->indexStart, sub->vertexOffset, sub->baseInstance);
  };

  // Synchronize
  // TODO: Improve this
  auto sync = [&](const SyncCmd*) {
    VkMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT |
                            VK_ACCESS_MEMORY_WRITE_BIT;

    const VkPipelineStageFlags srcStg = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkPipelineStageFlags dstStg = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkDependencyFlags depend = VK_DEPENDENCY_BY_REGION_BIT;

    // FIXME: This only affects the current render pass
    vkCmdPipelineBarrier(handle_, srcStg, dstStg, depend, 1, &barrier,
                         0, nullptr, 0, nullptr);
  };

  for (const auto& cmd : encoder.encoding()) {
    switch (cmd->cmd) {
    case Cmd::ViewportT:
      setViewport(static_cast<ViewportCmd*>(cmd.get()));
      break;
    case Cmd::ScissorT:
      setScissor(static_cast<ScissorCmd*>(cmd.get()));
      break;
    case Cmd::TargetT:
      setTarget(static_cast<TargetCmd*>(cmd.get()));
      break;
    case Cmd::StateGrT:
      setState(static_cast<StateGrCmd*>(cmd.get()));
      break;
    case Cmd::DcTableT:
      setDcTable(static_cast<DcTableCmd*>(cmd.get()));
      break;
    case Cmd::VxBufferT:
      setVxBuffer(static_cast<VxBufferCmd*>(cmd.get()));
      break;
    case Cmd::IxBufferT:
      setIxBuffer(static_cast<IxBufferCmd*>(cmd.get()));
      break;
    case Cmd::DrawT:
      draw(static_cast<DrawCmd*>(cmd.get()));
      break;
    case Cmd::DrawIxT:
      drawIx(static_cast<DrawIxCmd*>(cmd.get()));
      break;
    case Cmd::SyncT:
      sync(static_cast<SyncCmd*>(cmd.get()));
      break;
    default:
      assert(false);
      abort();
    }
  }

  if (tgt)
    endPass();
}

void CmdBufferVK::encode(const CpEncoder& encoder) {
  CpStateVK* cst = nullptr;
  vector<const DcTableCmd*> dtbs;

  // Set compute state
  auto setState = [&](const StateCpCmd* sub) {
    cst = &static_cast<CpStateVK&>(sub->state);
    auto pl = cst->pipeline();
    vkCmdBindPipeline(handle_, VK_PIPELINE_BIND_POINT_COMPUTE, pl);
  };

  // Set descriptor table
  auto setDcTable = [&](const DcTableCmd* sub) {
    dtbs.push_back(sub);
  };

  // Dispatch
  auto dispatch = [&](const DispatchCmd* sub) {
    if (!cst)
      throw invalid_argument("dispatch() requires a state to be set");

    if (!dtbs.empty()) {
      auto plLay = cst->plLayout();

      for (const auto& d : dtbs) {
        auto i = d->tableIndex;
        auto j = d->allocIndex;

        if (i >= cst->config().dcTables.size() ||
            j >= cst->config().dcTables[i]->allocations())
          throw invalid_argument("setDcTable() index out of range");

        auto ds = static_cast<DcTableVK*>(cst->config().dcTables[i])->ds(j);
        vkCmdBindDescriptorSets(handle_, VK_PIPELINE_BIND_POINT_COMPUTE,
                                plLay, i, 1, &ds, 0, nullptr);
      }

      dtbs.clear();
    }

    // TODO: Check limits
    vkCmdDispatch(handle_, sub->size.width, sub->size.height,
                  sub->size.depthOrLayers);
  };

  // Synchronize
  // TODO: Improve
  auto sync = [&](const SyncCmd*) {
    VkMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT |
                            VK_ACCESS_MEMORY_WRITE_BIT;

    const VkPipelineStageFlags srcStg = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkPipelineStageFlags dstStg = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkDependencyFlags depend = 0;

    vkCmdPipelineBarrier(handle_, srcStg, dstStg, depend, 1, &barrier,
                         0, nullptr, 0, nullptr);
  };

  for (const auto& cmd : encoder.encoding()) {
    switch (cmd->cmd) {
    case Cmd::StateCpT:
      setState(static_cast<StateCpCmd*>(cmd.get()));
      break;
    case Cmd::DcTableT:
      setDcTable(static_cast<DcTableCmd*>(cmd.get()));
      break;
    case Cmd::DispatchT:
      dispatch(static_cast<DispatchCmd*>(cmd.get()));
      break;
    case Cmd::SyncT:
      sync(static_cast<SyncCmd*>(cmd.get()));
      break;
    default:
      assert(false);
      abort();
    }
  }
}

void CmdBufferVK::encode(const TfEncoder& encoder) {
  // Copy buffer
  auto copyBB = [&](const CopyBBCmd* sub) {
    auto dst = &static_cast<BufferVK&>(sub->dst);
    auto src = &static_cast<BufferVK&>(sub->src);

    if (sub->size == 0 ||
        sub->dstOffset + sub->size > dst->size() ||
        sub->srcOffset + sub->size > src->size())
      throw invalid_argument("copy(buf, buf) invalid range");

    if (dst == src &&
        (uint64_t)abs((int64_t)(sub->srcOffset - sub->dstOffset)) < sub->size)
      throw invalid_argument("copy(buf, buf) memory overlap");

    VkBufferCopy region;
    region.srcOffset = sub->srcOffset;
    region.dstOffset = sub->dstOffset;
    region.size = sub->size;

    vkCmdCopyBuffer(handle_, src->handle(), dst->handle(), 1, &region);
  };

  // Copy image
  auto copyII = [&](const CopyIICmd* sub) {
    auto dst = &static_cast<ImageVK&>(sub->dst);
    auto src = &static_cast<ImageVK&>(sub->src);

    if (sub->size.width == 0 || sub->size.height == 0 || sub->layerCount == 0 ||
        sub->dstOffset.x + sub->size.width >
          dst->size().width >> sub->dstLevel ||
        sub->dstOffset.y + sub->size.height >
          dst->size().height >> sub->dstLevel ||
        sub->srcOffset.x + sub->size.width >
          src->size().width >> sub->srcLevel ||
        sub->srcOffset.y + sub->size.height >
          src->size().height >> sub->srcLevel ||
        sub->dstLayer + sub->layerCount > dst->size().depthOrLayers ||
        sub->srcLayer + sub->layerCount > src->size().depthOrLayers ||
        sub->dstLevel >= dst->levels() || sub->srcLevel >= src->levels())
      throw invalid_argument("copy(img, img) invalid range");

    if (dst->format() != src->format())
      throw invalid_argument("copy(img, img) formats differ");

    if (dst->samples() != src->samples())
      throw invalid_argument("copy(img, img) samples differ");

    // XXX
    dst->changeLayout(VK_IMAGE_LAYOUT_GENERAL, true);
    src->changeLayout(VK_IMAGE_LAYOUT_GENERAL, true);

    VkImageCopy region;
    region.srcSubresource.aspectMask = aspectOfVK(src->format());
    region.srcSubresource.mipLevel = sub->srcLevel;
    region.srcSubresource.baseArrayLayer = sub->srcLayer;
    region.srcSubresource.layerCount = sub->layerCount;
    region.srcOffset = {sub->srcOffset.x, sub->srcOffset.y, 0};
    region.dstSubresource.aspectMask = aspectOfVK(dst->format());
    region.dstSubresource.mipLevel = sub->dstLevel;
    region.dstSubresource.baseArrayLayer = sub->dstLayer;
    region.dstSubresource.layerCount = sub->layerCount;
    region.dstOffset = {sub->dstOffset.x, sub->dstOffset.y, 0};
    region.extent = {sub->size.width, sub->size.height, 1};

    vkCmdCopyImage(handle_, src->handle(), src->layout().second, dst->handle(),
                   dst->layout().second, 1, &region);
  };

  for (const auto& cmd : encoder.encoding()) {
    switch (cmd->cmd) {
    case Cmd::CopyBBT:
      copyBB(static_cast<CopyBBCmd*>(cmd.get()));
      break;
    case Cmd::CopyIIT:
      copyII(static_cast<CopyIICmd*>(cmd.get()));
      break;
    default:
      assert(false);
      abort();
    }
  }
}
