//
// SG
// UnitTests.h
//
// Copyright © 2020 Gustavo C. Viegas.
//

#ifndef YF_SG_UNITTESTS_H
#define YF_SG_UNITTESTS_H

#include "Test.h"

TEST_NS_BEGIN

Test* nodeTest();
Test* sceneTest();
Test* viewTest();
Test* vectorTest();
Test* matrixTest();
Test* meshTest();

inline const std::vector<Test*>& unitTests() {
  static std::vector<Test*> tests{nodeTest(), sceneTest(), viewTest(),
                                  vectorTest(), matrixTest(), meshTest()};
  return tests;
}

TEST_NS_END

#endif // YF_SG_UNITTESTS_H
