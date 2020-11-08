//
// cg
// DcTable.cxx
//
// Copyright © 2020 Gustavo C. Viegas.
//

#include "yf/cg/DcTable.h"

using namespace CG_NS;

DcTable::DcTable(const DcEntries& entries) : entries(entries) {}

DcTable::DcTable(DcEntries&& entries) : entries(entries) {}

DcTable::~DcTable() {}