//
// SG
// TextureImpl.h
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#ifndef YF_SG_TEXTUREIMPL_H
#define YF_SG_TEXTUREIMPL_H

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

#include "yf/cg/DcTable.h"

#include "Texture.h"

#ifdef YF_DEVEL
# include "../test/Test.h"
TEST_NS_BEGIN struct TextureTest; TEST_NS_END
#endif

SG_NS_BEGIN

/// Texture implementation details.
///
class Texture::Impl {
 public:
  Impl(const Data& data);
  Impl(const Impl& other, const CG_NS::Sampler& sampler, TexCoordSet coordSet);
  Impl(const Impl& other);
  Impl& operator=(const Impl& other);
  ~Impl();

  /// Getters.
  ///
  CG_NS::Sampler& sampler();
  const CG_NS::Sampler& sampler() const;
  TexCoordSet& coordSet();
  TexCoordSet coordSet() const;

  /// Updates image data.
  ///
  void updateImage(CG_NS::Offset2 offset, CG_NS::Size2 size, uint32_t level,
                   const void* data);

  /// Copies image data to a descriptor table.
  ///
  void copy(CG_NS::DcTable& dcTable, uint32_t allocation, CG_NS::DcId id,
            uint32_t element, uint32_t level);

 private:
  /// Key for the resource map.
  ///
  struct Key {
    CG_NS::PxFormat format = CG_NS::PxFormatUndefined;
    CG_NS::Size2 size{0};
    uint32_t levels = 1;
    CG_NS::Samples samples = CG_NS::Samples1;

    bool operator==(const Key& other) const {
      return format == other.format && size == other.size &&
             levels == other.levels && samples == other.samples;
    }

    Key() = default;
  };

  /// Key hasher.
  ///
  struct Hash {
    size_t operator()(const Key& key) const {
      const size_t f = key.format;
      const size_t w = key.size.width;
      const size_t h = key.size.height;
      const size_t l = key.levels;
      const size_t s = key.samples;
      return (f << 16) ^ ((w << 16) | h) ^ (l << 24) ^ (s << 8) ^ 0xDD1698C9;
    }
  };

  /// Image resource.
  ///
  struct Resource {
    CG_NS::Image::Ptr image;
    struct {
      std::vector<uint32_t> refCounts;
      uint32_t remaining;
      uint32_t current;
    } layers;
  };

  using Resources = std::unordered_map<Key, Resource, Hash>;
  static Resources resources_;

  Key key_{};
  uint32_t layer_ = UINT32_MAX;
  CG_NS::Sampler sampler_{};
  TexCoordSet coordSet_ = TexCoordSet0;

  bool setLayerCount(Resource&, uint32_t);

#ifdef YF_DEVEL
  friend TEST_NS::TextureTest;
#endif
};

SG_NS_END

#endif // YF_SG_TEXTUREIMPL_H
