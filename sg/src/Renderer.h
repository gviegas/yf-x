//
// SG
// Renderer.h
//
// Copyright © 2021 Gustavo C. Viegas.
//

#ifndef YF_SG_RENDERER_H
#define YF_SG_RENDERER_H

#include <vector>
#include <unordered_map>

#include "yf/cg/Pass.h"
#include "yf/cg/Shader.h"
#include "yf/cg/DcTable.h"
#include "yf/cg/State.h"
#include "yf/cg/Queue.h"
#include "yf/cg/Buffer.h"

#include "Scene.h"
#include "Model.h"

SG_NS_BEGIN

/// Renderer.
///
class Renderer {
 public:
  Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  ~Renderer() = default;

  /// Resource identifiers that shaders must abide by.
  ///
  static constexpr uint32_t GlbTable = 0;
  static constexpr uint32_t MdlTable = 1;
  static constexpr CG_NS::DcId Uniform = 0;
  static constexpr CG_NS::DcId ColorImgSampler = 1;
  static constexpr CG_NS::DcId MetalRoughImgSampler = 2;
  static constexpr CG_NS::DcId NormalImgSampler = 3;
  static constexpr CG_NS::DcId OcclusionImgSampler = 4;
  static constexpr CG_NS::DcId EmissiveImgSampler = 5;

  /// Shader pathnames.
  ///
  using Shader = std::pair<CG_NS::Stage, const wchar_t*>;
  static constexpr wchar_t ShaderDir[] = L"bin/";
  static constexpr Shader MdlShaders[]{{CG_NS::StageVertex, L"Mdl.vert"},
                                       {CG_NS::StageFragment, L"Mdl.frag"}};

  /// Renders a scene on a given target.
  ///
  void render(Scene& scene, CG_NS::Target& target);

 private:
  Scene* prevScene_{};
  CG_NS::Pass* prevPass_{};
  CG_NS::DcTable::Ptr glbTable_{};
  CG_NS::CmdBuffer::Ptr cmdBuffer_{};
  CG_NS::Buffer::Ptr unifBuffer_{};

  /// Key for the model map.
  ///
  struct MdlKey {
    Mesh* mesh{};
    Material* material{};

    bool operator==(const MdlKey& other) const {
      return mesh == other.mesh && material == other.material;
    }
  };

  /// Hasher for the model map.
  ///
  struct MdlHash {
    size_t operator()(const MdlKey& k) const {
      return std::hash<void*>()(k.mesh) ^ std::hash<void*>()(k.material);
    }
  };

  /// Value for the model map.
  ///
  using MdlValue = std::vector<Model*>;

  std::unordered_map<MdlKey, MdlValue, MdlHash> models_{};

  /// Resource for rendering.
  ///
  struct Resource {
    std::vector<CG_NS::Shader::Ptr> shaders{};
    CG_NS::DcTable::Ptr table{};
    CG_NS::GrState::Ptr state{};

    void reset() {
      state.reset();
      table.reset();
      shaders.clear();
    }
  };

  // TODO: resources for different kinds of models (e.g., points primitives)
  // TODO: resources for instanced draw
  Resource resource_{};

  /// Processes a scene graph.
  ///
  void processGraph(Scene& scene);

  /// Prepares for rendering.
  ///
  void prepare();
};

SG_NS_END

#endif // YF_SG_RENDERER_H