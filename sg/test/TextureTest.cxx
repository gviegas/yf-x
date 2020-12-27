//
// SG
// TextureTest.cxx
//
// Copyright © 2020 Gustavo C. Viegas.
//

#include <iostream>

#include "UnitTests.h"

#include "TextureImpl.h"

using namespace TEST_NS;
using namespace SG_NS;
using namespace std;

INTERNAL_NS_BEGIN

struct TextureTest : Test {
  TextureTest() : Test(L"Texture") { }

  Assertions run(const vector<string>&) {
    Assertions a;

    auto print = [] {
      wcout << "\n#Resources#\n";
      for (const auto& kv : Texture::Impl::resources_) {
        const auto& r = kv.second;
        wcout << "#image: " << r.image->format_ << ", "
              << r.image->size_.width << "x" << r.image->size_.height << ", "
              << r.image->layers_ << ", " << r.image->levels_ << ", "
              << r.image->samples_ << endl << "#layers: [ ";
        for (const auto& u : r.layers.unused)
          wcout << u << " ";
        wcout << "], " << r.layers.remaining << ", " << r.layers.current
              << endl;
      }
    };

    const auto& r = Texture::Impl::resources_;
    a.push_back({L"Texture::Impl::resources_", r.empty()});

    bool ctorChk = true;
    bool dtorChk = true;

    print();

    uint8_t b[1<<14];
    Texture::Data data{b, CG_NS::PxFormatRgb8Unorm, {32}};

    Texture t1(data);
    print();
    if (r.size() != 1) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+1 != e.layers.unused.size())
        ctorChk = false;
    }

    Texture t2(data);
    print();
    if (r.size() != 1) {
      ctorChk = false;
    } else {
      auto& e = r.find(t2.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.unused.size())
        ctorChk = false;
    }

    Texture t3(data);
    print();
    if (r.size() != 1) {
      ctorChk = false;
    } else {
      auto& e = r.find(t3.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.unused.size())
        ctorChk = false;
    }

    data.format = CG_NS::PxFormatR8Unorm;
    Texture t4(data);
    print();
    if (r.size() != 2) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.unused.size())
        ctorChk = false;
      auto& f = r.find(t4.impl().key_)->second;
      if (f.layers.remaining+1 != f.layers.unused.size())
        ctorChk = false;
    }

    Texture t5(data);
    print();
    if (r.size() != 2) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.unused.size())
        ctorChk = false;
      auto& f = r.find(t5.impl().key_)->second;
      if (f.layers.remaining+2 != f.layers.unused.size())
        ctorChk = false;
    }

    data.size.height = 16;
    Texture t6(data);
    print();
    if (r.size() != 3) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.unused.size())
        ctorChk = false;
      auto& f = r.find(t4.impl().key_)->second;
      if (f.layers.remaining+2 != f.layers.unused.size())
        ctorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.unused.size())
        ctorChk = false;
    }

    t3.~Texture();
    print();
    if (r.size() != 3) {
      dtorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.unused.size())
        dtorChk = false;
      auto& f = r.find(t4.impl().key_)->second;
      if (f.layers.remaining+2 != f.layers.unused.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.unused.size())
        dtorChk = false;
    }

    t4.~Texture();
    print();
    if (r.size() != 3) {
      dtorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.unused.size())
        dtorChk = false;
      auto& f = r.find(t5.impl().key_)->second;
      if (f.layers.remaining+1 != f.layers.unused.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.unused.size())
        dtorChk = false;
    }

    t5.~Texture();
    print();
    if (r.size() != 2) {
      dtorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.unused.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.unused.size())
        dtorChk = false;
    }

    data.format = CG_NS::PxFormatRgb8Unorm;
    data.size = {32};
    Texture t7(data);
    print();
    if (r.size() != 2) {
      dtorChk = false;
    } else {
      auto& e = r.find(t7.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.unused.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.unused.size())
        dtorChk = false;
    }

    a.push_back({L"Texture(Data)", ctorChk});
    a.push_back({L"~Texture()", dtorChk});
    return a;
  }
};

INTERNAL_NS_END

Test* TEST_NS::textureTest() {
  static TextureTest test;
  return &test;
}