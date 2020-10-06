//
// yf
// Test.h
//
// Copyright (C) 2020 Gustavo C. Viegas.
//

#ifndef YF_TEST_H
#define YF_TEST_H

#include <string>
#include <vector>

#include "YFDefs.h"

#define TEST_NS YF_NS::test
#define TEST_NS_BEGIN namespace TEST_NS {
#define TEST_NS_END }

TEST_NS_BEGIN

using Coverage = double;

class Test {
 public:
  explicit Test(std::wstring&& name);
  Test(const Test&) = delete;
  Test& operator=(const Test&) = delete;
  virtual ~Test();
  virtual Coverage run(const std::vector<std::string>& args) = 0;
  const std::wstring name;
};

Coverage run(const std::vector<Test*>& tests,
             std::vector<std::string>&& args = {});

TEST_NS_END

#endif // YF_TEST_H
