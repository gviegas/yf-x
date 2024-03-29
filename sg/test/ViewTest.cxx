//
// SG
// ViewTest.cxx
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#include <iostream>

#include "Test.h"
#include "View.h"
#include "Scene.h"

using namespace TEST_NS;
using namespace SG_NS;
using namespace std;

INTERNAL_NS_BEGIN

struct ViewTest : Test {
  ViewTest() : Test(L"View") { }

  Assertions run(const vector<string>&) {
    auto win = WS_NS::createWindow(400, 240, name_);
    Scene scn;

    View view(*win);

    for (uint32_t fps : {24, 30, 60}) {
      chrono::nanoseconds elapsed{};
      wcout << "\n<loop() [" << fps << " FPS]>\n";

      view.loop(scn, fps, [&](auto dt) {
        elapsed += dt;
        wcout << "(t) " << (double)dt.count()/1e9 << "\n";
        return elapsed.count() < 1'000'000'000;
      });
    }

    return {{L"View()", true}};
  }
};

INTERNAL_NS_END

TEST_NS_BEGIN

Test* viewTest() {
  static ViewTest test;
  return &test;
}

TEST_NS_END
