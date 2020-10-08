//
// yf
// Test.cxx
//
// Copyright (C) 2020 Gustavo C. Viegas.
//

#include <iostream>
#include <cmath>

#include "Test.h"

using namespace TEST_NS;
using namespace std;

Test::Test(wstring&& name) : name(name) {}

Test::~Test() {}

Coverage TEST_NS::run(const vector<Test*>& tests, vector<string>&& args)  {
  if (tests.size() == 0) {
    wcout << "\n\nNo tests to run\n";
    return 0.0;
  }

  auto runUnit = [&](Test& t) {
    wstring title = L" Unit test '" + t.name + L"' ";
    wstring tildes;
    tildes.resize(title.size());
    fill(tildes.begin(), tildes.end(), L'~');
    wcout << "\n\n" << tildes << tildes << tildes
          << "\n" << tildes << title << tildes
          << "\n" << tildes << tildes << tildes << "\n";

    auto asserts = t.run(args);

    Coverage cov = 0.0;
    for (auto& a : asserts) {
      wcout << "\n@ " << a.first << (a.second ? "\nPASSED\n" : "\nFAILED\n");
      cov += a.second;
    }
    cov = asserts.empty() ? 1.0 : cov/asserts.size();

    wstring dots;
    dots.resize(64*cov);
    fill(dots.begin(), dots.end(), L'.');
    wcout << "\n" << dots << " [" << cov*100.0 << "%]\n";

    return cov;
  };

  vector<pair<Test*, Coverage>> res;
  Coverage unit, total = 0.0;
  for (auto test : tests) {
    unit = runUnit(*test);
    total += unit;
    res.push_back({test, unit});
  }
  total /= tests.size();

  wcout << "\n\n<DONE>\n"
        << "\n\n------ SUMMARY ------\n---------------------\n";
  for (auto& p : res) {
    wcout << "\n[" << round(p.second*100.0) << "%]\t" << p.first->name;
  }
  wcout << "\n\n> Unit tests run: " << tests.size()
        << "\n\n> Test coverage: " << total*100.0 << "%\n";

  return total;
}
