//
// WS
// Run.cxx
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#include <iostream>

#include "UnitTests.h"

using namespace TEST_NS;
using namespace std;

int main(int argc, char* argv[]) {
  wstring line;
  line.resize(80);
  fill(line.begin(), line.end(), L'#');

  wcout << line << "\n[WS] Test\n" << line << "\n\n";

  vector<string> args;
  wcout << argv[0] << " ";
  for (int i = 1; i < argc; i++) {
    args.push_back(argv[i]);
    wcout << argv[i] << " ";
  }
  wcout << endl;

  run(unitTests(args.empty() ? "" : args[0]), move(args));

  wcout << "\n" << line << "\nEnd of test\n" << line << "\n";
}
