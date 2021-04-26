//
// SG
// Renderer.cxx
//
// Copyright © 2021 Gustavo C. Viegas.
//

#include <algorithm>
#include <typeinfo>
#include <stdexcept>
#include <cassert>

#include "yf/cg/Device.h"

#include "Renderer.h"
#include "Model.h"
#include "Material.h"
#include "TextureImpl.h"
#include "MeshImpl.h"
#include "Camera.h"

using namespace SG_NS;
using namespace std;

// TODO: consider allowing custom length values
constexpr uint64_t UnifLength = 1ULL << 14;
// TODO
constexpr uint64_t GlbLength = Mat4f::dataSize() << 1;
constexpr uint64_t MdlLength = Mat4f::dataSize() << 1;

Renderer::Renderer() {
  auto& dev = CG_NS::device();

  // One global table instance for shared uniforms
  const CG_NS::DcEntries glb{{Uniform, {CG_NS::DcTypeUniform, 1}}};
  glbTable_ = dev.dcTable(glb);
  glbTable_->allocate(1);

  cmdBuffer_ = dev.defaultQueue().cmdBuffer();

  unifBuffer_ = dev.buffer(UnifLength);
}

void Renderer::render(Scene& scene, CG_NS::Target& target) {
  auto pass = &target.pass();

  if (pass != prevPass_) {
    resource_.reset();
  } else if (&scene == prevScene_) {
    // TODO
  }

  prevScene_ = &scene;
  prevPass_ = pass;

  processGraph(scene);
  prepare();

  // Encode common commands
  CG_NS::GrEncoder enc;
  enc.setTarget(&target);
  enc.setViewport({0.0f, 0.0f, static_cast<float>(target.size_.width),
                   static_cast<float>(target.size_.height), 0.0f, 1.0f});
  enc.setScissor({{0}, target.size_});
  enc.setDcTable(GlbTable, 0);
  enc.clearColor(scene.color());
  enc.clearDepth(1.0f);

  // Update global uniform buffer
  uint64_t off = 0;
  uint64_t len;

  len = Mat4f::dataSize();
  unifBuffer_->write(off, len, scene.camera().view().data());
  off += len;
  unifBuffer_->write(off, len, scene.camera().projection().data());
  off += len;
  // TODO: other global data (light, viewport, ortho matrix, ...)

  glbTable_->write(0, Uniform, 0, *unifBuffer_, 0, off);

  // Render models
  auto renderMdl = [&] {
    vector<MdlKey> completed{};
    auto allocN = resource_.table ? resource_.table->allocations() : 0U;
    auto alloc2N = resource2_.table ? resource2_.table->allocations() : 0U;
    auto alloc4N = resource4_.table ? resource4_.table->allocations() : 0U;

    for (auto& kv : models_) {
      const auto size = kv.second.size();
      Resource* resource;
      uint32_t n;
      uint32_t alloc;

      if (size == 1) {
        if (allocN == 0)
          continue;
        resource = &resource_;
        n = 1;
        alloc = --allocN;
      } else if (size == 2) {
        if (alloc2N == 0)
          continue;
        resource = &resource2_;
        n = 2;
        alloc = --alloc2N;
      } else if (size <= 4) {
        if (alloc4N == 0)
          continue;
        resource = &resource4_;
        n = size;
        alloc = --alloc4N;
      } else {
        // TODO
        assert(false);
        abort();
      }

      enc.setState(resource->state.get());
      enc.setDcTable(MdlTable, alloc);

      auto matl = kv.second[0]->material();
      auto mesh = kv.second[0]->mesh();

      for (uint32_t i = 0; i < n; ++i) {
        auto mdl = kv.second.back();
        kv.second.pop_back();

        const auto& m = mdl->transform();
        const auto mv = scene.camera().view() * m;
        const auto beg = off;
        len = Mat4f::dataSize();
        unifBuffer_->write(off, len, m.data());
        off += len;
        unifBuffer_->write(off, len, mv.data());
        off += len;
        // TODO: other instance data

        resource->table->write(alloc, Uniform, i, *unifBuffer_, beg, off);
      }

      if (matl) {
        const pair<Texture*, CG_NS::DcId> texs[]{
          {matl->pbrmr().colorTex, ColorImgSampler},
          {matl->pbrmr().metalRoughTex, MetalRoughImgSampler},
          {matl->normal().texture, NormalImgSampler},
          {matl->occlusion().texture, OcclusionImgSampler},
          {matl->emissive().texture, EmissiveImgSampler}};

        for (const auto& tp : texs) {
          if (tp.first)
            tp.first->impl().copy(*resource->table, alloc, tp.second,
                                  0, 0, nullptr);
        }
        // TODO: also copy factors to uniform buffer
      } else {
        // TODO
        throw runtime_error("Cannot render models with no material set");
      }

      if (mesh)
        mesh->impl().encode(enc, 0, n);
      else
        // TODO
        throw runtime_error("Cannot render models with no mesh set");

      if (kv.second.empty())
        completed.push_back(kv.first);
    }

    for (const auto& k : completed)
      models_.erase(k);
  };

  // Render & submit
  do {
    renderMdl();
    cmdBuffer_->encode(enc);
    cmdBuffer_->enqueue();
    const_cast<CG_NS::Queue&>(cmdBuffer_->queue()).submit();
  } while (!models_.empty());
}

void Renderer::processGraph(Scene& scene) {
  models_.clear();

  if (scene.isLeaf())
    return;

  scene.traverse([&](Node& node) {
    if (typeid(node) == typeid(Model)) {
      auto& mdl = static_cast<Model&>(node);
      const MdlKey key{mdl.mesh(), mdl.material()};

      auto it = models_.find(key);
      if (it == models_.end())
        models_.emplace(key, MdlValue{&mdl});
      else
        it->second.push_back(&mdl);
    }
  }, true);
}

void Renderer::prepare() {
  auto& dev = CG_NS::device();

  // Set model resources and returns required uniform space
  auto setMdl = [&](Resource& resource, uint32_t instN, uint32_t allocN) {
    assert(instN > 0);
    assert(allocN > 0);

    if (resource.shaders.empty()) {
      switch (instN) {
      case 1:
        for (const auto& tp : MdlShaders)
          resource.shaders.push_back(dev.shader(tp.first,
                                                wstring(ShaderDir)+tp.second));
        break;
      case 2:
        for (const auto& tp : Mdl2Shaders)
          resource.shaders.push_back(dev.shader(tp.first,
                                                wstring(ShaderDir)+tp.second));
        break;
      case 4:
        for (const auto& tp : Mdl4Shaders)
          resource.shaders.push_back(dev.shader(tp.first,
                                                wstring(ShaderDir)+tp.second));
        break;
      default:
        assert(false);
        abort();
      }
    }

    if (!resource.table) {
      const CG_NS::DcEntries inst{
        {Uniform,              {CG_NS::DcTypeUniform,    instN}},
        {ColorImgSampler,      {CG_NS::DcTypeImgSampler, 1}},
        {MetalRoughImgSampler, {CG_NS::DcTypeImgSampler, 1}},
        {NormalImgSampler,     {CG_NS::DcTypeImgSampler, 1}},
        {OcclusionImgSampler,  {CG_NS::DcTypeImgSampler, 1}},
        {EmissiveImgSampler,   {CG_NS::DcTypeImgSampler, 1}}};
      resource.table = dev.dcTable(inst);
    }

    if (resource.table->allocations() != allocN)
      resource.table->allocate(allocN);

    if (!resource.state) {
      vector<CG_NS::Shader*> shd;
      for (const auto& s : resource.shaders)
        shd.push_back(s.get());

      const vector<CG_NS::DcTable*> tab{glbTable_.get(), resource.table.get()};

      const vector<CG_NS::VxInput> inp{vxInputFor(VxTypePosition),
                                       vxInputFor(VxTypeTangent),
                                       vxInputFor(VxTypeNormal),
                                       vxInputFor(VxTypeTexCoord0),
                                       vxInputFor(VxTypeTexCoord1),
                                       vxInputFor(VxTypeColor0),
                                       vxInputFor(VxTypeJoints0),
                                       vxInputFor(VxTypeWeights0)};

      resource.state = dev.state({prevPass_, shd, tab, inp,
                                  CG_NS::PrimitiveTriangle,
                                  CG_NS::PolyModeFill, CG_NS::CullModeBack,
                                  CG_NS::WindingCounterCw});
    }

    return MdlLength * instN * allocN;
  };

  // TODO: instanced rendering (> 4)
  if (any_of(models_.begin(), models_.end(),
             [](const auto& kv) { return kv.second.size() > 4; }))
    throw runtime_error("Instanced rendering of models (> 4) unimplemented");

  uint64_t unifLen = GlbLength;

  // Set models
  if (models_.empty()) {
    resource_.reset();
    resource2_.reset();
    resource4_.reset();
    // TODO: reset other resources when implemented
  } else {
    uint32_t mdlN = 0;
    uint32_t mdl2N = 0;
    uint32_t mdl4N = 0;
    for (const auto& kv : models_) {
      const auto size = kv.second.size();
      if (size == 1)
        ++mdlN;
      else if (size == 2)
        ++mdl2N;
      else if (size <= 4)
        ++mdl4N;
      else
        // TODO
        assert(false);
    }
    if (mdlN > 0)
      unifLen += setMdl(resource_, 1, mdlN);
    if (mdl2N > 0)
      unifLen += setMdl(resource2_, 2, mdl2N);
    if (mdl4N > 0)
      unifLen += setMdl(resource4_, 4, mdl4N);
    // TODO: other instanced draw models
  }

  unifLen = (unifLen & ~255) + 256;

  // TODO: improve resizing
  // TODO: also consider shrinking if buffer grows too much
  if (unifLen > unifBuffer_->size_)
    unifBuffer_ = dev.buffer(unifLen);
}
