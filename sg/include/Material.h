//
// SG
// Material.h
//
// Copyright © 2021 Gustavo C. Viegas.
//

#ifndef YF_SG_MATERIAL_H
#define YF_SG_MATERIAL_H

#include <cstddef>
#include <memory>

#include "yf/sg/Defs.h"
#include "yf/sg/Vector.h"
#include "yf/sg/Texture.h"

SG_NS_BEGIN

/// Material.
///
class Material {
 public:
  using Ptr = std::unique_ptr<Material>;

  /// PBR metallic-roughness.
  ///
  struct Pbrmr {
    Texture::Ptr colorTex{};
    Vec4f colorFac{1.0f, 1.0f, 1.0f, 1.0f};
    Texture::Ptr metalRoughTex{};
    float metallic = 1.0f;
    float roughness = 1.0f;
  };

  /// Normal map.
  ///
  struct Normal {
    Texture::Ptr texture{};
    float scale = 1.0f;
  };

  /// Occlusion map.
  ///
  struct Occlusion {
    Texture::Ptr texture{};
    float strength = 1.0f;
  };

  /// Emissive map.
  ///
  struct Emissive {
    Texture::Ptr texture{};
    Vec3f factor{};
  };

  /// Alpha modes.
  ///
  enum AlphaMode {
    Opaque,
    Blend,
    Mask
  };

  Material();
  Material(const Material& other);
  Material& operator=(const Material& other);
  ~Material();

  /// PBRMR.
  ///
  Pbrmr& pbrmr();
  const Pbrmr& pbrmr() const;

  /// Normal.
  ///
  Normal& normal();
  const Normal& normal() const;

  /// Occlusion.
  ///
  Occlusion& occlusion();
  const Occlusion& occlusion() const;

  /// Emissive.
  ///
  Emissive& emissive();
  const Emissive& emissive() const;

  /// Alpha mode.
  ///
  /// default: `Opaque`
  ///
  void setAlphaMode(AlphaMode alphaMode);
  AlphaMode alphaMode() const;

  /// Alpha cutoff (`Mask` alpha mode only).
  ///
  /// default: `0.5f`
  ///
  void setAlphaCutoff(float alphaCutoff);
  float alphaCutoff() const;

  /// Double sided.
  ///
  /// default: `false`
  ///
  void setDoubleSided(bool doubleSided);
  bool doubleSided() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

SG_NS_END

#endif // YF_SG_MATERIAL_H
