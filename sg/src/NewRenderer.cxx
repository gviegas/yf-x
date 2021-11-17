//
// SG
// NewRenderer.cxx
//
// Copyright © 2021 Gustavo C. Viegas.
//

#include <typeinfo>
#include <cstdio>
#include <cstring>

#include "yf/cg/Device.h"

#include "NewRenderer.h"
#include "Node.h"
#include "Scene.h"
#include "Camera.h"
#include "Model.h"
#include "MeshImpl.h"
#include "Skin.h"
#include "Material.h"

using namespace SG_NS;
using namespace std;

constexpr uint64_t UnifBufferSize = 1 << 21;
constexpr CG_NS::DcEntry GlobalUnif{0, CG_NS::DcTypeUniform, 1};
constexpr CG_NS::DcEntry LightUnif{1, CG_NS::DcTypeUniform, 1};
constexpr CG_NS::DcEntry InstanceUnif{0, CG_NS::DcTypeUniform, 1};
constexpr CG_NS::DcEntry MaterialUnif{1, CG_NS::DcTypeUniform, 1};
constexpr CG_NS::DcId FirstImgSampler = MaterialUnif.id + 1;

NewRenderer::NewRenderer() {
  auto& dev = CG_NS::device();

  cmdBuffer_ = dev.defaultQueue().cmdBuffer();
  unifBuffer_ = dev.buffer(UnifBufferSize);

  // This table will contain data common to all drawables
  mainTable_ = dev.dcTable({GlobalUnif, LightUnif});
  mainTable_->allocate(1);
}

void NewRenderer::render(Scene& scene, CG_NS::Target& target) {
  if (&scene == scene_) {
    // TODO
  }

  if (&target.pass() != pass_) {
    // TODO
    abort();
  }

  scene_ = &scene;
  pass_ = &target.pass();

  viewport_.width = target.size().width;
  viewport_.height = target.size().height;
  scissor_.size = target.size();

  processGraph();

  // TODO...
}

void NewRenderer::processGraph() {
  drawableNodes_.clear();
  blendDrawables_.clear();
  opaqueDrawables_.clear();

  if (scene_->isLeaf())
    return;

  scene_->worldTransform() = scene_->transform();
  scene_->worldInverse() = invert(scene_->worldTransform());

  const auto& mdlId = typeid(Model);

  scene_->traverse([&](Node& node) {
    node.worldTransform() = node.parent()->worldTransform() * node.transform();
    node.worldInverse() = invert(node.worldTransform());
    node.worldNormal() = transpose(node.worldInverse());

    const auto& id = typeid(node);
    if (id == mdlId) {
      auto& mdl = static_cast<Model&>(node);

      if (!mdl.mesh())
        throw runtime_error("Cannot render models with no mesh set");

      pushDrawables(node, *mdl.mesh(), mdl.skin());
    }
  }, true);
}

void NewRenderer::pushDrawables(Node& node, Mesh& mesh, Skin* skin) {
  const size_t nodeIndex = drawableNodes_.size();
  drawableNodes_.push_back(&node);

  for (size_t i = 0; i < mesh.primitiveCount(); i++) {
    const auto topology = mesh[i].topology();
    const auto dataMask = mesh[i].dataMask();
    const auto material = mesh[i].material();
    DrawableReqMask mask = 0;

    switch (topology) {
    case CG_NS::TopologyTriangle:
      break;
    case CG_NS::TopologyLine:
      mask |= RLine;
      break;
    case CG_NS::TopologyPoint:
      mask |= RPoint;
      break;
    case CG_NS::TopologyTriStrip:
      mask |= RTriStrip;
      break;
    case CG_NS::TopologyLnStrip:
      mask |= RLnStrip;
      break;
    case CG_NS::TopologyTriFan:
      mask |= RTriFan;
      break;
    }

    if (dataMask & VxDataNormal)
      mask |= RNormal;
    if (dataMask & VxDataTangent)
      mask |= RTangent;
    if (dataMask & VxDataTexCoord0)
      mask |= RTexCoord0;
    if (dataMask & VxDataTexCoord1)
      mask |= RTexCoord1;

    if (dataMask & VxDataJoints0) {
      if (!(dataMask & VxDataWeights0))
        throw runtime_error("Primitive has joint data but no weight data");
      if (!skin)
        throw runtime_error("Primitive has skinning data but no skin set");

      mask |= RSkin0;

    } else if (dataMask & VxDataWeights0) {
      throw runtime_error("Primitive has weight data but no joint data");
    }

    if (!material)
      throw runtime_error("Cannot render primitives with no material set");

    // TODO: PBRSG and Unlit materials

    if (material->pbrmr().colorTex)
      mask |= RColorMap;
    if (material->pbrmr().metalRoughTex)
      mask |= RPbrMap;
    if (material->normal().texture)
      mask |= RNormalMap;
    if (material->occlusion().texture)
      mask |= ROcclusionMap;
    if (material->emissive().texture)
      mask |= REmissiveMap;

    Drawable* drawable;
    if (material->alphaMode() == Material::Blend) {
      mask |= RAlphaBlend;

      // TODO: Sort
      blendDrawables_.push_back({nodeIndex, mesh[i], mask, UINT32_MAX});
      drawable = &blendDrawables_.back();

    } else {
      if (material->alphaMode() == Material::Mask)
        mask |= RAlphaMask;
      // Opaque alpha mode otherwise

      opaqueDrawables_.push_back({nodeIndex, mesh[i], mask, UINT32_MAX});
      drawable = &opaqueDrawables_.back();
    }

    if (!setState(*drawable))
      // TODO
      throw runtime_error("Could not set state for Drawable)");
  }
}

bool NewRenderer::setState(Drawable& drawable) {
  const auto stateIndex = getIndex(drawable.mask, states_);

  if (!stateIndex.second) {
    CG_NS::GrState::Config config;
    config.pass = pass_;

    uint32_t vertIndex, fragIndex, tableIndex;
    if (!setShaders(drawable.mask, config, vertIndex, fragIndex) ||
        !setTables(drawable.mask, config, tableIndex))
      return false;

    setInputs(drawable.mask, config);
    config.topology = drawable.primitive.topology();
    config.polyMode = CG_NS::PolyModeFill;
    config.cullMode = drawable.mask & RAlphaBlend ?
                      CG_NS::CullModeNone : CG_NS::CullModeBack;
    config.winding = CG_NS::WindingCounterCw;

    try {
      states_.insert(states_.begin() + stateIndex.first,
                     {CG_NS::device().state(config), 0, drawable.mask,
                      vertIndex, fragIndex, tableIndex});
    } catch (...) {
      return false;
    }
  }

  drawable.stateIndex = stateIndex.first;
  states_[stateIndex.first].count++;

  return true;
}

bool NewRenderer::setShaders(DrawableReqMask mask,
                             CG_NS::GrState::Config& config,
                             uint32_t& vertShaderIndex,
                             uint32_t& fragShaderIndex) {
  mask = mask & RShaderMask;

  const auto vertIndex = getIndex(mask, vertShaders_);
  const auto fragIndex = getIndex(mask, fragShaders_);

  auto shaderPath = [&](const char* format) {
    const auto n = snprintf(nullptr, 0, format, mask);
    if (n <= 0)
      throw runtime_error("Could not create shader path string");
    string str;
    str.resize(n + 1);
    snprintf(str.data(), str.size(), format, mask);
    return str;
  };

  if (!vertIndex.second) {
    try {
      const auto str = shaderPath("%X.vert.bin");
      vertShaders_.insert(vertShaders_.begin() + vertIndex.first,
                          {CG_NS::device().shader(CG_NS::StageVertex, str),
                           0, mask});
    } catch (...) {
      return false;
    }
  }

  if (!fragIndex.second) {
    try {
      const auto str = shaderPath("%X.frag.bin");
      vertShaders_.insert(fragShaders_.begin() + fragIndex.first,
                          {CG_NS::device().shader(CG_NS::StageFragment, str),
                           0, mask});
    } catch (...) {
      return false;
    }
  }

  auto& vertShader = vertShaders_[vertIndex.first];
  auto& fragShader = fragShaders_[fragIndex.first];
  config.shaders.push_back(vertShader.shader.get());
  config.shaders.push_back(fragShader.shader.get());
  vertShaderIndex = vertIndex.first;
  fragShaderIndex = fragIndex.first;
  vertShader.count++;
  fragShader.count++;

  return true;
}

bool NewRenderer::setTables(DrawableReqMask mask,
                            CG_NS::GrState::Config& config,
                            uint32_t& tableIndex) {
  mask = mask & RTableMask;

  const auto index = getIndex(mask, tables_);

  if (!index.second) {
    vector<CG_NS::DcEntry> entries{InstanceUnif, MaterialUnif};

    CG_NS::DcId id = FirstImgSampler;
    auto imgSampler = [&] {
      return CG_NS::DcEntry{id++, CG_NS::DcTypeImgSampler, 1};
    };

    if (mask & RColorMap)
      entries.push_back(imgSampler());

    if (!(mask & RUnlit)) {
      // PBRMR or PBRSG
      if (mask & RPbrMap)
        entries.push_back(imgSampler());
      if (mask & RNormalMap)
        entries.push_back(imgSampler());
      if (mask & ROcclusionMap)
        entries.push_back(imgSampler());
      if (mask & REmissiveMap)
        entries.push_back(imgSampler());
    }

    try {
      tables_.insert(tables_.begin() + index.first,
                     {CG_NS::device().dcTable(entries), 0, mask});
    } catch (...) {
      return false;
    }
  }

  config.dcTables.push_back(mainTable_.get());

  auto& table = tables_[index.first];
  config.dcTables.push_back(table.table.get());
  tableIndex = index.first;
  table.count++;

  return true;
}

void NewRenderer::setInputs(DrawableReqMask mask,
                            CG_NS::GrState::Config& config) {

  config.vxInputs.push_back(vxInputFor(VxDataPosition));
  if (mask & RNormal)
    config.vxInputs.push_back(vxInputFor(VxDataNormal));
  if (mask & RTangent)
    config.vxInputs.push_back(vxInputFor(VxDataTangent));
  if (mask & RTexCoord0)
    config.vxInputs.push_back(vxInputFor(VxDataTexCoord0));
  if (mask & RTexCoord1)
    config.vxInputs.push_back(vxInputFor(VxDataTexCoord1));
  if (mask & RColor0)
    config.vxInputs.push_back(vxInputFor(VxDataColor0));
  if (mask & RSkin0) {
    config.vxInputs.push_back(vxInputFor(VxDataJoints0));
    config.vxInputs.push_back(vxInputFor(VxDataWeights0));
  }
}

void NewRenderer::writeGlobal(uint64_t& offset) {
  Global global;
  const auto& cam = scene_->camera();

  memcpy(global.v, cam.view().data(), sizeof global.v);
  memcpy(global.p, cam.projection().data(), sizeof global.p);
  memcpy(global.vp, cam.transform().data(), sizeof global.vp);
  memcpy(global.o, ortho(1.0f, 1.0f, 0.0f, -1.0f).data(), sizeof global.o);

  if (ViewportN > 1)
    // TODO
    throw runtime_error("Cannot render to multiple viewports");

  global.vport[0].x = 0.0f;
  global.vport[0].y = 0.0f;
  global.vport[0].width = viewport_.width;
  global.vport[0].height = viewport_.height;
  global.vport[0].zNear = viewport_.zNear;
  global.vport[0].zFar = viewport_.zFar;
  global.vport[0].pad1 = 0.0f;

  const uint64_t size = sizeof global;
  unifBuffer_->write(offset, size, &global);
  mainTable_->write(0, GlobalUnif.id, 0, *unifBuffer_, offset, size);
  // TODO: Align
  offset += size;
}

void NewRenderer::writeLight(uint64_t& offset) {
  // TODO: Light nodes not implemented yet
  Light light;
  light.l[0].notUsed = 1;

  const uint64_t size = sizeof light;
  unifBuffer_->write(offset, size, &light);
  mainTable_->write(0, LightUnif.id, 0, *unifBuffer_, offset, size);
  // TODO: Align
  offset += size;
}
