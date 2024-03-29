//
// CG
// Image.cxx
//
// Copyright © 2020-2023 Gustavo C. Viegas.
//

#include <stdexcept>

#include "Image.h"

using namespace CG_NS;
using namespace std;

// TODO: Validate parameters here rather than on backend.
Image::Image(const Desc& desc)
  : format_(desc.format), size_(desc.size), levels_(desc.levels),
    samples_(desc.samples), dimension_(desc.dimension),
    usageMask_(desc.usageMask) { }

Format Image::format() const {
  return format_;
}

Size3 Image::size() const {
  return size_;
}

uint32_t Image::levels() const {
  return levels_;
}

Samples Image::samples() const {
  return samples_;
}

Image::Dimension Image::dimension() const {
  return dimension_;
}

Image::UsageMask Image::usageMask() const {
  return usageMask_;
}

uint32_t Image::texelSize(Format format) {
  switch (format) {
  case Format::R8Unorm:
  case Format::R8Norm:
  case Format::R8Uint:
  case Format::R8Int:
  case Format::S8:
    return 1;

  case Format::R16Uint:
  case Format::R16Int:
  case Format::R16Float:
  case Format::Rg8Unorm:
  case Format::Rg8Norm:
  case Format::Rg8Uint:
  case Format::Rg8Int:
  case Format::D16Unorm:
    return 2;

  case Format::R32Uint:
  case Format::R32Int:
  case Format::R32Float:
  case Format::Rg16Uint:
  case Format::Rg16Int:
  case Format::Rg16Float:
  case Format::Rgba8Unorm:
  case Format::Rgba8Srgb:
  case Format::Rgba8Norm:
  case Format::Rgba8Uint:
  case Format::Rgba8Int:
  case Format::Bgra8Unorm:
  case Format::Bgra8Srgb:
  case Format::Rgb10a2Unorm:
  case Format::Rg11b10Float:
  case Format::D32Float:
  case Format::D24UnormS8:
    return 4;

  case Format::Rg32Uint:
  case Format::Rg32Int:
  case Format::Rg32Float:
  case Format::Rgba16Uint:
  case Format::Rgba16Int:
  case Format::Rgba16Float:
    return 8;

  case Format::Rgba32Uint:
  case Format::Rgba32Int:
  case Format::Rgba32Float:
    return 16;

  case Format::D32FloatS8:
    return 5;

  case Format::Undefined:
  default:
    throw invalid_argument(__func__);
  }
}

uint32_t Image::texelSize() const {
  return texelSize(format());
}

// TODO: Validate parameters here rather than on backend.
ImgView::ImgView(Image& image, const Desc& desc)
  : image_(image), levels_(desc.levels), layers_(desc.layers),
    dimension_(desc.dimension) { }

ImgView::~ImgView() { }

Image& ImgView::image() {
  return image_;
}

Range ImgView::levels() const {
  return levels_;
}

Range ImgView::layers() const {
  return layers_;
}

ImgView::Dimension ImgView::dimension() const {
  return dimension_;
}
