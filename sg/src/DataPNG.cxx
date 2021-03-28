//
// SG
// DataPNG.cxx
//
// Copyright © 2021 Gustavo C. Viegas.
//

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <cassert>
#include <fstream>
#include <atomic>
#include <algorithm>

#include "DataPNG.h"
#include "yf/Except.h"

#if defined(_DEFAULT_SOURCE)
# include <endian.h>
inline uint16_t letoh(uint16_t v) { return le16toh(v); }
inline uint32_t letoh(uint32_t v) { return le32toh(v); }
inline uint16_t htole(uint16_t v) { return htole16(v); }
inline uint32_t htole(uint32_t v) { return htole16(v); }
inline uint16_t betoh(uint16_t v) { return be16toh(v); }
inline uint32_t betoh(uint32_t v) { return be32toh(v); }
inline uint16_t htobe(uint16_t v) { return htobe16(v); }
inline uint32_t htobe(uint32_t v) { return htobe16(v); }
#else
// TODO
# error "Invalid platform"
#endif

using namespace YF_NS;
using namespace SG_NS;
using namespace std;

INTERNAL_NS_BEGIN

/// Node of a code tree.
///
struct ZNode {
  bool isLeaf;
  union {
    uint16_t next[2];
    uint32_t value;
  };

  ZNode() : isLeaf(false), next{0, 0} { }
  ZNode(uint16_t next0, uint16_t next1) : isLeaf(false), next{next0, next1} { }
  ZNode(uint32_t value) : isLeaf(true), value(value) { }

  constexpr const uint16_t& operator[](size_t index) const {
    assert(index <= 1);
    return next[index];
  };

  constexpr uint16_t& operator[](size_t index) {
    assert(index <= 1);
    return next[index];
  }
};

/// Code tree.
///
using ZTree = vector<ZNode>;

/// Creates a code tree from ordered code lengths.
///
void createCodeTree(const vector<uint8_t>& codeLengths, ZTree& codeTree) {
  assert(!codeLengths.empty());
  assert(codeTree.empty());

  // Count number of codes per length
  const auto maxLen = *max_element(codeLengths.begin(), codeLengths.end());
  vector<uint32_t> count{};
  count.resize(maxLen + 1);
  for (const auto& len : codeLengths)
    ++count[len];

  // Set initial codes for each length
  count[0] = 0;
  vector<uint32_t> nextCode{};
  nextCode.resize(maxLen + 1);
  for (uint8_t i = 1; i <= maxLen; ++i)
    nextCode[i] = (nextCode[i-1] + count[i-1]) << 1;

  // Create tree
  codeTree.push_back({});
  for (uint32_t i = 0; i < codeLengths.size(); ++i) {
    if (codeLengths[i] == 0)
      continue;

    const auto length = codeLengths[i];
    const auto code = nextCode[length]++;

    uint16_t node = 0;
    for (uint8_t j = 0; j < length; ++j) {
      const uint8_t bit = (code >> (length-j-1)) & 1;
      if (codeTree[node][bit] == 0) {
        codeTree[node][bit] = codeTree.size();
        codeTree.push_back({});
      }
      node = codeTree[node][bit];
    }

    codeTree.back() = {i};
  }
}

#ifdef YF_DEVEL
void printCodeTree(const ZTree& codeTree);
#endif

/// Decompresses data.
///
void inflate(const vector<uint8_t>& src, vector<uint8_t>& dst) {
  assert(src.size() > 2);

  // Datastream header
  struct {
    uint8_t cm:4, cinfo:4;
    uint8_t fcheck:5, fdict:1, flevel:2;
  } hdr alignas(uint16_t);
  static_assert(sizeof hdr == 2, "!sizeof");

  memcpy(&hdr, src.data(), sizeof hdr);

  if (hdr.cm != 8 || hdr.cinfo > 7 || hdr.fdict != 0 ||
      betoh(*reinterpret_cast<uint16_t*>(&hdr)) % 31 != 0)
    throw runtime_error("Invalid data for decompression");

  // Process blocks
  size_t dataOff = 0;
  size_t byteOff = 2;
  size_t bitOff = 0;

  auto nextBit = [&] {
    assert(bitOff < 8);
    const uint8_t bit = (src[byteOff] >> (7-bitOff)) & 1;
    const div_t d = div(++bitOff, 8);
    byteOff += d.quot;
    bitOff = d.rem;
    return bit;
  };

  while (true) {
    const auto bfinal = nextBit();
    const auto btype = (nextBit() << 1) | nextBit();

    // No compression
    if (btype == 0) {
      if (bitOff != 0) {
        ++byteOff;
        bitOff = 0;
      }

      uint16_t len;
      memcpy(&len, &src[byteOff], sizeof len);
      len = letoh(len);
      byteOff += 2;

      uint16_t nlen;
      memcpy(&nlen, &src[byteOff], sizeof nlen);
      nlen = letoh(nlen);
      byteOff += 2;

      if ((nlen ^ len) != 0xFFFF)
        throw runtime_error("Invalid data for decompression");

      assert(dataOff + len <= dst.size());

      memcpy(&dst[dataOff], &src[byteOff], len);
      dataOff += len;
      byteOff += len;

    // Compression
    } else {
      if (btype == 1) {
        // Fixed H. codes
        // TODO
      } else if (btype == 2) {
        // Dynamic H. codes
        // TODO
      } else {
        throw runtime_error("Invalid data for decompression");
      }
    }

    if (bfinal)
      break;
  }
}

/// PNG.
///
class PNG {
 public:
  /// File signature.
  ///
  static constexpr uint8_t Signature[]{137, 80, 78, 71, 13, 10, 26, 10};

  /// Chunk names.
  ///
  static constexpr uint8_t IHDRType[]{'I', 'H', 'D', 'R'};
  static constexpr uint8_t PLTEType[]{'P', 'L', 'T', 'E'};
  static constexpr uint8_t IDATType[]{'I', 'D', 'A', 'T'};
  static constexpr uint8_t IENDType[]{'I', 'E', 'N', 'D'};

  /// IHDR.
  ///
  struct IHDR {
    uint32_t width;
    uint32_t height;
    uint8_t bitDepth;
    uint8_t colorType;
    uint8_t compressionMethod;
    uint8_t filterMethod;
    uint8_t interlaceMethod;
  };
  static constexpr uint32_t IHDRSize = 13;
  static_assert(offsetof(IHDR, interlaceMethod) == IHDRSize-1, "!offsetof");

  /// Computes CRC.
  ///
  uint32_t computeCRC(const char* data, uint32_t n) const {
    assert(data);
    assert(n > 0);

    static uint32_t table[256]{};
    static atomic pending{true};
    static bool wait = true;

    if (pending.exchange(false)) {
      for (uint32_t i = 0; i < 256; ++i) {
        auto x = i;
        for (uint32_t j = 0; j < 8; ++j)
          x = (x & 1) ? (0xEDB88320 ^ (x >> 1)) : (x >> 1);
        table[i] = x;
      }
      wait = false;
    } else {
      while (wait) { }
    }

    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < n; ++i)
      crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFF;
  }

  PNG(const std::wstring& pathname) : ihdr_{}, plte_{}, idat_{} {
    // Convert pathname string
    string path{};
    size_t len = (pathname.size() + 1) * sizeof(wchar_t);
    path.resize(len);
    const wchar_t* wsrc = pathname.data();
    mbstate_t state;
    memset(&state, 0, sizeof state);
    len = wcsrtombs(path.data(), &wsrc, path.size(), &state);
    if (wsrc || static_cast<size_t>(-1) == len)
      throw ConversionExcept("Could not convert PNG file path");
    path.resize(len);

    ifstream ifs(path);
    if (!ifs)
      throw FileExcept("Could not open PNG file");

    // Check signature
    uint8_t sign[sizeof Signature];

    if (!ifs.read(reinterpret_cast<char*>(sign), sizeof sign))
      throw FileExcept("Could not read from PNG file");

    if (memcmp(sign, Signature, sizeof sign) != 0)
      throw FileExcept("Invalid PNG file");

    // Process chunks
    vector<char> buffer{};
    buffer.resize(4096);

    const uint32_t lengthOff = 0;
    const uint32_t typeOff = 4;
    const uint32_t dataOff = 8;
    uint32_t length;
    uint32_t type;
    uint32_t crc;

    auto setLength = [&] {
      memcpy(&length, &buffer[lengthOff], sizeof length);
      length = betoh(length);
      const auto required = dataOff + length + sizeof crc;
      if (required > buffer.size())
        buffer.resize(required);
    };

    while (true) {
      // Read length and type
      if (!ifs.read(buffer.data(), dataOff))
        throw FileExcept("Could not read from PNG file");

      setLength();

      // Read data and CRC
      if (!ifs.read(&buffer[dataOff], length + sizeof crc))
        throw FileExcept("Could not read from PNG file");

      // Check CRC
      memcpy(&crc, &buffer[dataOff+length], sizeof crc);
      crc = betoh(crc);
      if (crc != computeCRC(&buffer[typeOff], length + sizeof type))
        throw FileExcept("Invalid CRC for PNG file");

      // IHDR
      if (memcmp(&buffer[typeOff], IHDRType, sizeof IHDRType) == 0) {
        if (length < IHDRSize)
          throw FileExcept("Invalid PNG file");

        memcpy(&ihdr_, &buffer[dataOff], length);
        ihdr_.width = betoh(ihdr_.width);
        ihdr_.height = betoh(ihdr_.height);

      // PLTE
      } else if (memcmp(&buffer[typeOff], PLTEType, sizeof PLTEType) == 0) {
        if (length % 3 != 0 || plte_.size() != 0)
          throw FileExcept("Invalid PNG file");

        plte_.resize(length);
        memcpy(plte_.data(), &buffer[dataOff], length);

      // IDAT
      } else if (memcmp(&buffer[typeOff], IDATType, sizeof IDATType) == 0) {
        if (length > 0) {
          const auto offset = idat_.size();
          idat_.resize(offset + length);
          memcpy(&idat_[offset], &buffer[dataOff], length);
        }

      // IEND
      } else if (memcmp(&buffer[typeOff], IENDType, sizeof IENDType) == 0) {
        break;

      // XXX: cannot ignore critical chunks
      } else if (!(buffer[typeOff] & 32)) {
        throw UnsupportedExcept("Unsupported PNG file");
      }
    }

    if (ihdr_.width == 0 || ihdr_.height == 0 ||
        ihdr_.compressionMethod != 0 || ihdr_.filterMethod != 0 ||
        ihdr_.interlaceMethod > 1 || idat_.empty())
      throw FileExcept("Invalid PNG file");
  }

  PNG(const PNG&) = delete;
  PNG& operator=(const PNG&) = delete;
  ~PNG() = default;

 private:
  IHDR ihdr_{};
  vector<uint8_t> plte_{};
  vector<uint8_t> idat_{};

#ifdef YF_DEVEL
  friend void printPNG(const PNG& png);
#endif
};

INTERNAL_NS_END

void SG_NS::loadPNG(Texture::Data& dst, const std::wstring& pathname) {
  PNG png(pathname);

#ifdef YF_DEVEL
  printPNG(png);
#endif

  // TODO
#ifdef YF_DEVEL
  ZTree tree;
  createCodeTree({3, 3, 3, 3, 3, 2, 4, 4}, tree);
  printCodeTree(tree);
  exit(0);
#endif
}

//
// DEVEL
//

#ifdef YF_DEVEL

INTERNAL_NS_BEGIN

void printCodeTree(const ZTree& codeTree) {
  wprintf(L"\nCode Tree");
  for (size_t i = 0; i < codeTree.size(); ++i) {
    wprintf(L"\n (%lu) ", i);
    if (codeTree[i].isLeaf)
      wprintf(L"value: %u", codeTree[i].value);
    else
      wprintf(L"next: %u/%u", codeTree[i][0], codeTree[i][1]);
  }
  wprintf(L"\n");
}

void printPNG(const PNG& png) {
  wprintf(L"\nPNG");
  wprintf(L"\n IHDR:");
  wprintf(L"\n  width: %u", png.ihdr_.width);
  wprintf(L"\n  height: %u", png.ihdr_.height);
  wprintf(L"\n  bitDepth: %u", png.ihdr_.bitDepth);
  wprintf(L"\n  colorType: %u", png.ihdr_.colorType);
  wprintf(L"\n  compressionMethod: %u", png.ihdr_.compressionMethod);
  wprintf(L"\n  filterMethod: %u", png.ihdr_.filterMethod);
  wprintf(L"\n  interlaceMethod: %u", png.ihdr_.interlaceMethod);
  wprintf(L"\n PLTE: %lu byte(s)", png.plte_.size());
  wprintf(L"\n IDAT: %lu byte(s)", png.idat_.size());
  wprintf(L"\n");
}

INTERNAL_NS_END

#endif
