//
// yf
// CGDevice.h
//
// Copyright © 2020 Gustavo C. Viegas.
//

#ifndef YF_CG_DEVICE_H
#define YF_CG_DEVICE_H

#include <memory>
#include <cstdint>

#include "YFDefs.h"
#include "CGBuffer.h"
#include "CGImage.h"
#include "CGShader.h"
#include "CGDcTable.h"
#include "CGPass.h"
#include "CGState.h"
#include "CGQueue.h"

YF_NS_BEGIN

class CGDevice {
 public:
  virtual ~CGDevice();

  /// Retrieves the device instance.
  ///
  static CGDevice& get();

  /// Makes a new buffer object.
  ///
  using BufferPtr = std::unique_ptr<CGBuffer>;
  virtual BufferPtr makeBuffer(uint64_t size) = 0;

  /// Makes a new image object.
  ///
  using ImagePtr = std::unique_ptr<CGImage>;
  virtual ImagePtr makeImage(CGPxFormat format,
                             CGSize2 size,
                             uint32_t layers,
                             uint32_t levels,
                             CGSamples samples) = 0;

  /// Makes a new shader object.
  ///
  using ShaderPtr = std::unique_ptr<CGShader>;
  virtual ShaderPtr makeShader(CGStage stage,
                               std::wstring&& codeFile,
                               std::wstring&& entryPoint = L"main") = 0;

  /// Makes a new descriptor table object.
  ///
  using DcTablePtr = std::unique_ptr<CGDcTable>;
  virtual DcTablePtr makeDcTable(const CGDcEntries& entries) = 0;
  virtual DcTablePtr makeDcTable(CGDcEntries&& entries) = 0;

  /// Makes a new pass object.
  ///
  using PassPtr = std::unique_ptr<CGPass>;
  virtual PassPtr makePass(const std::vector<CGColorAttach>* colors,
                           const std::vector<CGColorAttach>* resolves,
                           const CGDepStenAttach* depthStencil) = 0;

  /// Makes a new state object.
  ///
  using GrStatePtr = std::unique_ptr<CGGrState>;
  using CpStatePtr = std::unique_ptr<CGCpState>;
  virtual GrStatePtr makeState(const CGGrState::Config& config) = 0;
  virtual GrStatePtr makeState(CGGrState::Config&& config) = 0;
  virtual CpStatePtr makeState(const CGCpState::Config& config) = 0;
  virtual CpStatePtr makeState(CGCpState::Config&& config) = 0;

  /// Retrieves execution queues.
  ///
  virtual CGQueue& defaultQueue() = 0;
  virtual CGQueue* queue(CGQueue::CapabilityMask capabilities) = 0;
};

YF_NS_END

#endif // YF_CG_DEVICE_H
