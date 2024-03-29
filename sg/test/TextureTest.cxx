//
// SG
// TextureTest.cxx
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#include <iostream>

#include "Test.h"
#include "TextureImpl.h"

using namespace SG_NS;
using namespace std;

TEST_NS_BEGIN

struct TextureTest : Test {
  TextureTest() : Test(L"Texture") { }

  Assertions run(const vector<string>&) {
    Assertions a;

    auto print = [] {
      wcout << "\nResources\n";
      for (const auto& kv : Texture::Impl::resources_) {
        const auto& r = kv.second;
        wcout << " image: " << r.image->format() << ", "
              << r.image->size().width << "x" << r.image->size().height << ", "
              << r.image->layers() << ", " << r.image->levels() << ", "
              << r.image->samples() << endl << " layers: [ ";
        for (const auto& c : r.layers.refCounts)
          wcout << c << " ";
        wcout << "], " << r.layers.remaining << ", " << r.layers.current
              << endl;
      }
    };

    const auto& r = Texture::Impl::resources_;
    a.push_back({L"Texture::Impl::resources_", r.empty()});

    bool ctorChk = true;
    bool sharChk = true;
    bool dtorChk = true;

    print();

    Texture::Data data{make_unique<char[]>(1<<14), CG_NS::PxFormatRgb8Unorm,
                       {32}, 1, CG_NS::Samples1};

    Texture t1(data);
    print();
    if (r.size() != 1) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+1 != e.layers.refCounts.size())
        ctorChk = false;
    }

    Texture t2(data);
    print();
    if (r.size() != 1) {
      ctorChk = false;
    } else {
      auto& e = r.find(t2.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.refCounts.size())
        ctorChk = false;
    }

    Texture* t3 = new Texture(data);
    print();
    if (r.size() != 1) {
      ctorChk = false;
    } else {
      auto& e = r.find(t3->impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.refCounts.size())
        ctorChk = false;
    }

    data.format = CG_NS::PxFormatR8Unorm;
    Texture* t4 = new Texture(data);
    print();
    if (r.size() != 2) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.refCounts.size())
        ctorChk = false;
      auto& f = r.find(t4->impl().key_)->second;
      if (f.layers.remaining+1 != f.layers.refCounts.size())
        ctorChk = false;
    }

    Texture* t5 = new Texture(data);
    print();
    if (r.size() != 2) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.refCounts.size())
        ctorChk = false;
      auto& f = r.find(t5->impl().key_)->second;
      if (f.layers.remaining+2 != f.layers.refCounts.size())
        ctorChk = false;
    }

    data.size.height = 16;
    Texture t6(data);
    print();
    if (r.size() != 3) {
      ctorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+3 != e.layers.refCounts.size())
        ctorChk = false;
      auto& f = r.find(t4->impl().key_)->second;
      if (f.layers.remaining+2 != f.layers.refCounts.size())
        ctorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.refCounts.size())
        ctorChk = false;
    }

    delete t3;
    print();
    if (r.size() != 3) {
      dtorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.refCounts.size())
        dtorChk = false;
      auto& f = r.find(t4->impl().key_)->second;
      if (f.layers.remaining+2 != f.layers.refCounts.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.refCounts.size())
        dtorChk = false;
    }

    delete t4;
    print();
    if (r.size() != 3) {
      dtorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.refCounts.size())
        dtorChk = false;
      auto& f = r.find(t5->impl().key_)->second;
      if (f.layers.remaining+1 != f.layers.refCounts.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.refCounts.size())
        dtorChk = false;
    }

    delete t5;
    print();
    if (r.size() != 2) {
      dtorChk = false;
    } else {
      auto& e = r.find(t1.impl().key_)->second;
      if (e.layers.remaining+2 != e.layers.refCounts.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.refCounts.size())
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
      if (e.layers.remaining+3 != e.layers.refCounts.size())
        dtorChk = false;
      auto& g = r.find(t6.impl().key_)->second;
      if (g.layers.remaining+1 != g.layers.refCounts.size())
        dtorChk = false;
    }

    Texture t8(t2, {}, TexCoordSet1);
    print();
    if (r.size() != 2) {
      sharChk = false;
    } else {
      auto& a = r.find(t8.impl().key_)->second;
      auto& b = r.find(t2.impl().key_)->second;
      if (&a != &b)
        sharChk = false;
      if (t8.impl().layer_ != t2.impl().layer_)
        sharChk = false;
      if (a.layers.refCounts[t8.impl().layer_] != 2)
        sharChk = false;
      if (a.layers.remaining+3 != a.layers.refCounts.size())
        sharChk = false;
    }

    Texture t9(t2, {}, TexCoordSet1);
    print();
    if (r.size() != 2) {
      sharChk = false;
    } else {
      auto& a = r.find(t9.impl().key_)->second;
      auto& b = r.find(t2.impl().key_)->second;
      if (&a != &b)
        sharChk = false;
      if (t9.impl().layer_ != t2.impl().layer_)
        sharChk = false;
      if (a.layers.refCounts[t2.impl().layer_] != 3)
        sharChk = false;
      if (a.layers.remaining+3 != a.layers.refCounts.size())
        sharChk = false;
    }

    Texture* t10 = new Texture(t9, {}, TexCoordSet1);
    print();
    if (r.size() != 2) {
      sharChk = false;
    } else {
      auto& a = r.find(t10->impl().key_)->second;
      auto& b = r.find(t9.impl().key_)->second;
      if (&a != &b)
        sharChk = false;
      if (t10->impl().layer_ != t9.impl().layer_)
        sharChk = false;
      if (a.layers.refCounts[t9.impl().layer_] != 4)
        sharChk = false;
      if (a.layers.remaining+3 != a.layers.refCounts.size())
        sharChk = false;
    }

    Texture* t11 = new Texture(t1, {}, TexCoordSet1);
    print();
    if (r.size() != 2) {
      sharChk = false;
    } else {
      auto& a = r.find(t11->impl().key_)->second;
      auto& b = r.find(t1.impl().key_)->second;
      if (&a != &b)
        sharChk = false;
      if (t11->impl().layer_ != t1.impl().layer_)
        sharChk = false;
      if (a.layers.refCounts[t1.impl().layer_] != 2)
        sharChk = false;
      if (a.layers.remaining+3 != a.layers.refCounts.size())
        sharChk = false;
    }

    Texture* t12 = new Texture(t6, {}, TexCoordSet1);
    print();
    if (r.size() != 2) {
      sharChk = false;
    } else {
      auto& a = r.find(t12->impl().key_)->second;
      auto& b = r.find(t6.impl().key_)->second;
      if (&a != &b)
        sharChk = false;
      if (t12->impl().layer_ != t6.impl().layer_)
        sharChk = false;
      if (a.layers.refCounts[t1.impl().layer_] != 2)
        sharChk = false;
      if (a.layers.remaining+1 != a.layers.refCounts.size())
        sharChk = false;
    }

    delete t10;
    print();
    delete t11;
    print();
    delete t12;
    print();

    a.push_back({L"Texture(Data)", ctorChk});
    a.push_back({L"Texture(texture, sampler, coordSet)", sharChk});
    a.push_back({L"~Texture()", dtorChk});

    return a;
  }
};

Test* textureTest() {
  static TextureTest test;
  return &test;
}

TEST_NS_END
