//
// CG
// DrawTest.cxx
//
// Copyright © 2020-2023 Gustavo C. Viegas.
//

#include <chrono>

#include <algorithm>
#include <cassert>

#include "Test.h"
#include "Device.h"
#include "Encoder.h"

using namespace TEST_NS;
using namespace CG_NS;
using namespace std;

INTERNAL_NS_BEGIN

struct DrawTest : Test {
  DrawTest() : Test(L"Draw") { }

  bool draw() {
    // Get device and queue
    auto& dev = device();
    auto& que = dev.defaultQueue();

    // Create shaders
    auto vert = dev.shader({StageVertex, "main", "test/data/vert"});
    auto frag = dev.shader({StageFragment, "main", "test/data/frag"});

    // Create wsi
    auto win = WS_NS::createWindow(480, 400, name_);
    Size2 winSz{win->width(), win->height()};
    auto wsi = dev.wsi(*win);
    assert(wsi->size() > 0);

    // Create pass
    vector<AttachDesc> passClrs{{(*wsi)[0]->format(), (*wsi)[0]->samples()}};
    AttachDesc passDs{Format::D16Unorm, Samples1};
    auto pass = dev.pass(&passClrs, nullptr, &passDs);

    // Create depth/stencil image
    auto ds = dev.image({passDs.format, {winSz, 1}, 1, passDs.samples,
                         Image::Dim2, Image::Attachment});

    // Create a target for each wsi image
    vector<AttachImg> clrImgs{{nullptr, 0, 0}};
    AttachImg dsImg{ds.get(), 0, 0};
    vector<Target::Ptr> tgts;
    for (const auto& wi : *wsi) {
      clrImgs[0].image = wi;
      tgts.push_back(pass->target(winSz, 1, &clrImgs, nullptr, &dsImg));
    }

    // Create buffer and fill with data
    struct Vertex { float pos[3]; float tc[2]; };
    const Vertex vxData[] = {{{-1.0f,  1.0f, 0.5f}, {0.0f, 0.0f}},
                             {{ 1.0f,  1.0f, 0.5f}, {1.0f, 0.0f}},
                             {{ 0.0f, -1.0f, 0.5f}, {0.5f, 1.0f}}};

    const float unifData[] = {0.9f, 0.0f, 0.0f, 0.0f,
                              0.0f, 0.9f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.9f, 0.0f,
                              0.0f, 0.0f, 0.0f, 1.0f};

    const uint32_t vxOff = offsetof(Vertex, tc);
    const uint32_t vxStrd = sizeof(Vertex);

    const uint64_t unifAlign = dev.limits().minDcUniformWriteAlignedOffset;
    const uint64_t unifOff = sizeof vxData % unifAlign ?
                             unifAlign - (sizeof vxData % unifAlign) : 0;

    auto buf = dev.buffer({2048, Buffer::Shared, Buffer::Vertex |
                                                 Buffer::Uniform});
    buf->write(0, vxData, sizeof vxData);
    buf->write(sizeof vxData + unifOff, unifData, sizeof unifData);

    // Create sampling image and fill with data
    const uint32_t lyrN = 4;
    const uint8_t pxData[lyrN][4] = {
      {255, 255, 0, 255},
      {255, 0, 255, 255},
      {0, 255, 255, 255},
      {255, 255, 255, 255},
    };
    auto tex = dev.image({Format::Rgba8Unorm, {1, 1, lyrN}, 1, Samples1,
                          Image::Dim2, Image::CopyDst | Image::Sampled});
    tex->write(0, {}, 0, pxData, {1, 1, lyrN});

    // Create descriptor table, allocate resources and copy data
    const vector<DcEntry> dcs{{0, DcTypeUniform, 1}, {1, DcTypeImgSampler, 1}};
    auto dtb = dev.dcTable(dcs);
    dtb->allocate(lyrN);
    for (uint32_t i = 0; i < lyrN; i++) {
      dtb->write(i, 0, 0, *buf, sizeof vxData + unifOff, sizeof unifData);
      dtb->write(i, 1, 0, *tex, i, 0);
    }

    // Define vertex input
    const VxInput vxIn = {{{0, VxFormatFlt3, 0}, {1, VxFormatFlt2, vxOff}},
                          vxStrd, VxStepFnVertex};

    // Create graphics state
    GrState::Config config{
      pass.get(),
      {vert.get(), frag.get()},
      {dtb.get()},
      {vxIn},
      TopologyTriangle,
      PolyModeFill,
      CullModeBack,
      WindingCounterCw
    };
    auto state = dev.state(config);

    // Create command buffer
    auto cb = que.cmdBuffer();

    TargetOp tgtOp;
    tgtOp.colorOps.push_back({LoadOpClear, StoreOpStore});
    tgtOp.colorValues.push_back({0.005f, 0.005f, 0.005f, 1.0f});
    tgtOp.depthOp = {LoadOpClear, StoreOpStore};
    tgtOp.depthValue = 1.0f;

    // Enter rendering loop
    const auto t0 = chrono::system_clock::now();
    const auto tm = t0 + chrono::seconds(5);
    while (chrono::system_clock::now() < tm) {
      // Acquire next drawable image
      pair<Image*, Wsi::Index> img;
      while (!(img = wsi->nextImage()).first) { }

      auto tgtIt = find_if(tgts.begin(), tgts.end(), [&](auto& tgt) {
        return tgt->colors()->front().image == img.first;
      });
      assert(tgtIt != tgts.end() && tgtIt == tgts.begin() + img.second);

      const auto nm = chrono::system_clock::now() - t0;
      const auto dv = chrono::nanoseconds(chrono::milliseconds(666));
      const auto lyr = (nm / dv) % lyrN;

      // Encode commands
      GrEncoder enc;
      enc.setViewport({0.0f, 0.0f, (float)winSz.width, (float)winSz.height,
                       0.0f, 1.0f});
      enc.setScissor({{}, winSz});
      enc.setTarget(**tgtIt, tgtOp);
      enc.setState(*state);
      enc.setDcTable(0, lyr);
      enc.setVertexBuffer(*buf, 0, 0);
      enc.draw(0, 3, 0, 1);

      // Apply encoding to command buffer
      cb->encode(enc);

      // Enqueue command buffer
      cb->enqueue();

      // Submit for execution
      que.submit();

      // Present image
      wsi->present(img.second);
    }

    return true;
  }

  Assertions run(const vector<string>&) {
    return {{L"draw()", draw()}};
  }
};

INTERNAL_NS_END

TEST_NS_BEGIN

Test* drawTest() {
  static DrawTest test;
  return &test;
}

TEST_NS_END
