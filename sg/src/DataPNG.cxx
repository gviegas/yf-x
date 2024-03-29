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
#include <atomic>
#include <algorithm>
#include <vector>
#include <stdexcept>

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

#ifdef YF_DEVEL
# include <cstdio>
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
    count[len]++;

  // Set initial codes for each length
  count[0] = 0;
  vector<uint32_t> nextCode{};
  nextCode.resize(maxLen + 1);
  for (uint8_t i = 1; i <= maxLen; i++)
    nextCode[i] = (nextCode[i-1] + count[i-1]) << 1;

  // Create tree
  codeTree.push_back({});
  for (uint32_t i = 0; i < codeLengths.size(); i++) {
    if (codeLengths[i] == 0)
      continue;

    const auto length = codeLengths[i];
    const auto code = nextCode[length]++;

    uint16_t node = 0;
    for (uint8_t j = 0; j < length; j++) {
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

void printCodeTree(const ZTree& codeTree);

/// Decompresses data.
///
void inflate(const vector<uint8_t>& src, vector<uint8_t>& dst) {
  assert(src.size() > 2);
  assert(!dst.empty());

  // Datastream header
  struct {
    uint8_t cm:4, cinfo:4;
    uint8_t fcheck:5, fdict:1, flevel:2;
  } hdr;
  static_assert(sizeof hdr == 2, "!sizeof");

  memcpy(&hdr, src.data(), sizeof hdr);

  if (hdr.cm != 8 || hdr.cinfo > 7 || hdr.fdict != 0 ||
      ((src.data()[0] << 8) + src.data()[1]) % 31 != 0)
    throw runtime_error("Invalid data for decompression");

  // Process blocks
  size_t dataOff = 0;
  size_t byteOff = 2;
  size_t bitOff = 0;

  auto nextBit = [&] {
    assert(bitOff < 8);
    const uint8_t bit = (src[byteOff] >> bitOff) & 1;
    const div_t d = div(++bitOff, 8);
    byteOff += d.quot;
    bitOff = d.rem;
    return bit;
  };

  while (true) {
    const auto bfinal = nextBit();
    const auto btype = nextBit() | (nextBit() << 1);

    if (btype == 0) {
      // No compression
      if (bitOff != 0) {
        byteOff++;
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

    } else {
      // Compression
      ZTree literals{};
      ZTree distances{};

      if (btype == 1) {
        // Fixed H. codes
        vector<uint8_t> litLengths{};
        litLengths.resize(288);
        fill_n(litLengths.begin(), 144, 8);
        fill_n(litLengths.begin()+144, 112, 9);
        fill_n(litLengths.begin()+256, 24, 7);
        fill_n(litLengths.begin()+280, 8, 8);

        vector<uint8_t> distLengths{};
        distLengths.resize(32, 5);

        createCodeTree(litLengths, literals);
        createCodeTree(distLengths, distances);

      } else if (btype == 2) {
        // Dynamic H. codes
        uint16_t hlit = 0;
        for (uint16_t i = 0; i < 5; i++)
          hlit |= nextBit() << i;
        hlit += 257;

        uint16_t hdist = 0;
        for (uint16_t i = 0; i < 5; i++)
          hdist |= nextBit() << i;
        hdist++;

        uint16_t hclen = 0;
        for (uint16_t i = 0; i < 4; i++)
          hclen |= nextBit() << i;
        hclen += 4;

        // Create code length tree
        static constexpr uint8_t lenMap[19]{16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                            11, 4, 12, 3, 13, 2, 14, 1, 15};
        vector<uint8_t> lenLengths{};
        lenLengths.resize(19);
        for (uint16_t i = 0; i < hclen; i++)
          lenLengths[lenMap[i]] = nextBit() | (nextBit()<<1) | (nextBit()<<2);
        ZTree lenTree{};
        createCodeTree(lenLengths, lenTree);

        // Decompress code lengths using the code length tree
        auto decompressLengths = [&](vector<uint8_t>& lengths, uint16_t n) {
          uint16_t count = 0;
          while (count < n) {
            uint16_t node = 0;
            do
              node = lenTree[node][nextBit()];
            while (!lenTree[node].isLeaf);
            const auto value = lenTree[node].value;

            if (value < 16) {
              // Code length
              lengths.push_back(value);
              count++;
            } else if (value == 16) {
              // Copy previous
              uint8_t times = 3 + (nextBit() | (nextBit() << 1));
              count += times;
              while (times--)
                lengths.push_back(lengths.back());
            } else {
              // Repeat zero length
              uint8_t times = 0;
              uint8_t bits = 0;
              if (value == 17) {
                times = 3;
                bits = 3;
              } else if (value == 18) {
                times = 11;
                bits = 7;
              } else {
                throw runtime_error("Invalid data for decompression");
              }
              for (uint8_t i = 0; i < bits; i++)
                times += nextBit() << i;
              count += times;
              lengths.resize(lengths.size() + times);
            }
          }
        };

        // Decompress literal & distance code lengths and create code trees
        vector<uint8_t> litLengths{};
        vector<uint8_t> distLengths{};
        decompressLengths(litLengths, hlit);
        decompressLengths(distLengths, hdist);
        createCodeTree(litLengths, literals);
        createCodeTree(distLengths, distances);

      } else {
        throw runtime_error("Invalid data for decompression");
      }

      // Decompress data using the literal and distance code trees
      while (true) {
        uint16_t node;
        uint32_t value;

        // Decode literal/length
        node = 0;
        do
          node = literals[node][nextBit()];
        while (!literals[node].isLeaf);
        value = literals[node].value;

        if (value < 256) {
          // Literal
          assert(dataOff + 1 <= dst.size());
          dst[dataOff++] = value;

        } else if (value == 256) {
          // End of block
          break;

        } else if (value < 286) {
          // Length/Distance pair
          uint8_t extraBits = 0;

          // Compute length
          uint16_t length = 0;
          if (value <= 264) {
            extraBits = 0;
            length = 10 - (264-value);
          } else if (value <= 284) {
            extraBits = 1 + ((value-265) >> 2);
            length = 3 + (4 << extraBits) + (((value-265) & 3) << extraBits);
          } else {
            extraBits = 0;
            length = 258;
          }
          for (uint8_t i = 0; i < extraBits; i++)
            length += nextBit() << i;

          // Decode distance
          node = 0;
          do
            node = distances[node][nextBit()];
          while (!distances[node].isLeaf);
          value = distances[node].value;

          // Compute distance
          uint16_t distance = 0;
          if (value <= 3) {
            extraBits = 0;
            distance = value + 1;
          } else {
            extraBits = 1 + ((value-4) >> 1);
            distance = 1 + (2 << extraBits) + (((value-4) & 1) << extraBits);
          }
          for (uint8_t i = 0; i < extraBits; i++)
            distance += nextBit() << i;

          assert(distance <= dataOff);
          assert(dataOff + length <= dst.size());

          // Copy data
          while (length--) {
            dst[dataOff] = dst[dataOff-distance];
            dataOff++;
          }

        } else {
          throw runtime_error("Invalid file for decompression");
        }
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
  PNG(const string& pathname)
    : ihdr_{}, plte_{}, idat_{},
      components_(0), bpp_(0), Bpp_(0), sclnSize_(0) {

    ifstream ifs(pathname);
    if (!ifs)
      throw FileExcept("Could not open PNG file");

    init(ifs);
  }

  PNG(ifstream& ifs)
    : ihdr_{}, plte_{}, idat_{},
      components_(0), bpp_(0), Bpp_(0), sclnSize_(0) {

    init(ifs);
  }

  PNG(const PNG&) = delete;
  PNG& operator=(const PNG&) = delete;
  ~PNG() = default;

  /// Produces raw image data.
  ///
  unique_ptr<char[]> imageData() const {
    vector<uint8_t> cdata{};
    decompress(cdata);
    unfilter(cdata);

    const size_t lnSize = ihdr_.width * (ihdr_.colorType == 3 ? 3 : Bpp_);
    const size_t size = lnSize * ihdr_.height;
    auto idata = make_unique<char[]>(size);

    // Palette indices
    if (ihdr_.colorType == 3) {
      // XXX: Needs testing
      for (uint32_t i = 0; i < ihdr_.height; i++) {
        const auto scanline = &cdata[1+i*sclnSize_];
        uint32_t byteOff = 0;
        uint32_t bitOff = 0;
        for (uint32_t j = 0; j < ihdr_.width; j++) {
          uint8_t index = 0;
          for (uint8_t k = 0; k < ihdr_.bitDepth; k++)
            index |= scanline[byteOff] & (1 << (k+bitOff));
          index >>= bitOff;
          memcpy(&idata[i*lnSize+j*3], &plte_[index], 3);
          const div_t d = div(bitOff + ihdr_.bitDepth, 8);
          byteOff += d.quot;
          bitOff = d.rem;
        }
      }

    // 1/2/4 bit depth greyscale
    } else if (ihdr_.bitDepth < 8) {
      // XXX: Needs testing
      for (uint32_t i = 0; i < ihdr_.height; i++) {
        const auto scanline = &cdata[1+i*sclnSize_];
        uint32_t byteOff = 0;
        uint32_t bitOff = 0;
        for (uint32_t j = 0; j < ihdr_.width; j++) {
          uint8_t value = 0;
          for (uint8_t k = 0; k < ihdr_.bitDepth; k++)
            value |= scanline[byteOff] & (1 << (k+bitOff));
          idata[i*lnSize+j] = value >> bitOff;
          const div_t d = div(bitOff + ihdr_.bitDepth, 8);
          byteOff += d.quot;
          bitOff = d.rem;
        }
      }

    // 8/16 bit depth truecolor or greyscale
    } else {
      for (uint32_t i = 0; i < ihdr_.height; i++)
        memcpy(&idata[i*lnSize], &cdata[1+i*sclnSize_], lnSize);
      if (ihdr_.bitDepth == 16) {
        for (size_t i = 0; i < size; i += 2) {
          uint16_t x;
          memcpy(&x, &idata[i], 2);
          x = betoh(x);
          memcpy(&idata[i], &x, 2);
        }
      }
    }

    return idata;
  }

  /// Width of `imageData()`.
  ///
  uint32_t width() const {
    return ihdr_.width;
  }

  /// Height of `imageData()`.
  ///
  uint32_t height() const {
    return ihdr_.height;
  }

  /// Format of `imageData()`.
  ///
  CG_NS::PxFormat format() const {
    switch (ihdr_.colorType) {
    case 0:
      if (ihdr_.bitDepth == 16)
        return CG_NS::PxFormatR16Unorm;
      return CG_NS::PxFormatR8Unorm;

    case 2:
      if (ihdr_.bitDepth == 16)
        return CG_NS::PxFormatRgb16Unorm;
      //return CG_NS::PxFormatRgb8Unorm;
      return CG_NS::PxFormatRgb8Srgb;

    case 3:
      //return CG_NS::PxFormatRgb8Unorm;
      return CG_NS::PxFormatRgb8Srgb;

    case 4:
      if (ihdr_.bitDepth == 16)
        return CG_NS::PxFormatRg16Unorm;
      return CG_NS::PxFormatRg8Unorm;

    case 6:
      if (ihdr_.bitDepth == 16)
        return CG_NS::PxFormatRgba16Unorm;
      //return CG_NS::PxFormatRgba8Unorm;
      return CG_NS::PxFormatRgba8Srgb;

    default:
      break;
    }

    // XXX: Should never happen
    return CG_NS::PxFormatUndefined;
  }

  void print() const;

 private:
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

  IHDR ihdr_{};
  vector<uint8_t> plte_{};
  vector<uint8_t> idat_{}; // XXX: Concatenation of chunks

  uint32_t components_ = 0;
  uint32_t bpp_ = 0;
  uint32_t Bpp_ = 0;
  uint32_t sclnSize_ = 0; // XXX: Including filter byte

  /// Initializes PNG data from file stream.
  ///
  void init(ifstream& ifs) {
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

      // Copy data
      if (memcmp(&buffer[typeOff], IHDRType, sizeof IHDRType) == 0) {
        // IHDR
        if (length < IHDRSize)
          throw FileExcept("Invalid PNG file");

        memcpy(&ihdr_, &buffer[dataOff], length);
        ihdr_.width = betoh(ihdr_.width);
        ihdr_.height = betoh(ihdr_.height);

      } else if (memcmp(&buffer[typeOff], PLTEType, sizeof PLTEType) == 0) {
        // PLTE
        if (length % 3 != 0 || plte_.size() != 0)
          throw FileExcept("Invalid PNG file");

        plte_.resize(length);
        memcpy(plte_.data(), &buffer[dataOff], length);

      } else if (memcmp(&buffer[typeOff], IDATType, sizeof IDATType) == 0) {
        // IDAT
        if (length > 0) {
          const auto offset = idat_.size();
          idat_.resize(offset + length);
          memcpy(&idat_[offset], &buffer[dataOff], length);
        }

      } else if (memcmp(&buffer[typeOff], IENDType, sizeof IENDType) == 0) {
        // IEND
        break;

      } else if (!(buffer[typeOff] & 32)) {
        // XXX: Cannot ignore critical chunks
        throw UnsupportedExcept("Unsupported PNG file");
      }
    }

    // Validate
    if (ihdr_.width == 0 || ihdr_.height == 0 ||
        ihdr_.compressionMethod != 0 || ihdr_.filterMethod != 0 ||
        ihdr_.interlaceMethod > 1 || idat_.empty())
      throw FileExcept("Invalid PNG file");

    if (ihdr_.colorType == 2 || ihdr_.colorType == 4 || ihdr_.colorType == 6) {
      if (ihdr_.bitDepth != 8 && ihdr_.bitDepth != 16)
        throw FileExcept("Invalid PNG file");
    } else if (ihdr_.colorType == 0) {
      if (ihdr_.bitDepth != 1 && ihdr_.bitDepth != 2 &&
          ihdr_.bitDepth != 4 && ihdr_.bitDepth != 8 &&
          ihdr_.bitDepth != 16)
        throw FileExcept("Invalid PNG file");
    } else if (ihdr_.colorType == 3) {
      if (plte_.empty() || (ihdr_.bitDepth != 1 && ihdr_.bitDepth != 2 &&
                            ihdr_.bitDepth != 4 && ihdr_.bitDepth != 8))
        throw FileExcept("Invalid PNG file");
    } else {
      throw FileExcept("Invalid PNG file");
    }

    // TODO
    if (ihdr_.interlaceMethod != 0)
      throw UnsupportedExcept("Interlaced PNG images not supported");

    // Set auxiliar data members
    switch (ihdr_.colorType) {
    case 0:
    case 3:
      components_ = 1;
      break;
    case 2:
      components_ = 3;
      break;
    case 4:
      components_ = 2;
      break;
    case 6:
      components_ = 4;
      break;
    default:
      throw FileExcept("Invalid PNG file");
    }

    bpp_ = components_ * ihdr_.bitDepth;
    Bpp_ = max(bpp_ >> 3, 1U);

    if (bpp_ & 7) {
      // XXX: Scanlines must begin at byte boundaries
      const div_t d = div(ihdr_.width * bpp_, 8);
      sclnSize_ = 1 + d.quot + (d.rem != 0);
    } else {
      sclnSize_ = 1 + ihdr_.width * Bpp_;
    }
  }

  /// Decompresses concatenated IDAT datastream.
  ///
  void decompress(vector<uint8_t>& dst) const {
    assert(dst.empty());

    dst.resize(sclnSize_ * ihdr_.height);
    inflate(idat_, dst);
  }

  /// Reverses filters from decompressed data.
  ///
  void unfilter(vector<uint8_t>& data) const {
    assert(!data.empty());

    uint8_t* curScln = nullptr;
    uint8_t* priorScln = nullptr;
    uint8_t filter = UINT8_MAX;

    // Don't have prior scanline
    auto reverse0 = [&] {
      switch (filter) {
      case 0:
      case 2:
        // None/Up
        break;

      case 1:
      case 4:
        // Sub/Paeth
        for (uint32_t i = Bpp_+1; i < sclnSize_; i++)
          curScln[i] += curScln[i-Bpp_];
        break;

      case 3:
        // Average
        for (uint32_t i = Bpp_+1; i < sclnSize_; i++)
          curScln[i] += curScln[i-Bpp_] >> 1;
        break;

      default:
        throw runtime_error("Invalid PNG data for unfiltering");
      }
    };

    // Have prior scanline
    auto reverse = [&] {
      switch (filter) {
      case 0:
        // None
        break;

      case 1:
        // Sub
        for (uint32_t i = Bpp_+1; i < sclnSize_; i++)
          curScln[i] += curScln[i-Bpp_];
        break;

      case 2:
        // Up
        for (uint32_t i = 1; i < sclnSize_; i++)
          curScln[i] += priorScln[i];
        break;

      case 3:
        // Average
        for (uint32_t i = 1; i <= Bpp_; i++)
          curScln[i] += priorScln[i] >> 1;
        for (uint32_t i = Bpp_+1; i < sclnSize_; i++) {
          const uint16_t prev = curScln[i-Bpp_];
          const uint16_t prior = priorScln[i];
          curScln[i] += (prev + prior) >> 1;
        }
        break;

      case 4:
        // Paeth
        for (uint32_t i = 1; i <= Bpp_; i++)
          curScln[i] += priorScln[i];
        for (uint32_t i = Bpp_+1; i < sclnSize_; i++) {
          const int16_t a = curScln[i-Bpp_];
          const int16_t b = priorScln[i];
          const int16_t c = priorScln[i-Bpp_];
          const int16_t p = a + b - c;
          const int16_t pa = abs(p-a);
          const int16_t pb = abs(p-b);
          const int16_t pc = abs(p-c);
          curScln[i] += (pa <= pb && pa <= pc) ? (a) : (pb <= pc ? b : c);
        }
        break;

      default:
        throw runtime_error("Invalid PNG data for unfiltering");
      }
    };

    curScln = data.data();
    filter = curScln[0];
    reverse0();
    for (uint32_t i = 1; i < ihdr_.height; i++) {
      priorScln = curScln;
      curScln = &data[i*sclnSize_];
      filter = curScln[0];
      reverse();
    }
  }

  /// Computes the CRC of a chunk.
  ///
  uint32_t computeCRC(const char* data, uint32_t n) const {
    assert(data);
    assert(n > 0);

    static uint32_t table[256]{};
    static atomic pending{true};
    static bool wait = true;

    if (pending.exchange(false)) {
      for (uint32_t i = 0; i < 256; i++) {
        auto x = i;
        for (uint32_t j = 0; j < 8; j++)
          x = (x & 1) ? (0xEDB88320 ^ (x >> 1)) : (x >> 1);
        table[i] = x;
      }
      wait = false;
    } else {
      while (wait) { }
    }

    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < n; i++)
      crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFF;
  }
};

INTERNAL_NS_END

void SG_NS::loadPNG(Texture::Data& dst, const string& pathname) {
  PNG png(pathname);

  png.print();

  dst.data = png.imageData();
  dst.format = png.format();
  dst.size = {png.width(), png.height()};
  dst.levels = 1;
  dst.samples = CG_NS::Samples1;
}

void SG_NS::loadPNG(Texture::Data& dst, ifstream& stream) {
  PNG png(stream);

  png.print();

  dst.data = png.imageData();
  dst.format = png.format();
  dst.size = {png.width(), png.height()};
  dst.levels = 1;
  dst.samples = CG_NS::Samples1;
}

//
// DEVEL
//

INTERNAL_NS_BEGIN

void PNG::print() const {
#ifdef YF_DEVEL_PNG
  wprintf(L"\nPNG");
  wprintf(L"\n IHDR:");
  wprintf(L"\n  width: %u", ihdr_.width);
  wprintf(L"\n  height: %u", ihdr_.height);
  wprintf(L"\n  bitDepth: %u", ihdr_.bitDepth);
  wprintf(L"\n  colorType: %u", ihdr_.colorType);
  wprintf(L"\n  compressionMethod: %u", ihdr_.compressionMethod);
  wprintf(L"\n  filterMethod: %u", ihdr_.filterMethod);
  wprintf(L"\n  interlaceMethod: %u", ihdr_.interlaceMethod);
  wprintf(L"\n PLTE: %lu byte(s)", plte_.size());
  wprintf(L"\n IDAT: %lu byte(s)", idat_.size());
  wprintf(L"\n *Aux.:");
  wprintf(L"\n  *Components: %u", components_);
  wprintf(L"\n  *bpp: %u", bpp_);
  wprintf(L"\n  *Bpp: %u", Bpp_);
  wprintf(L"\n  *sclnSize: %u", sclnSize_);
  wprintf(L"\n  *format: %d", format());
  wprintf(L"\n");
#endif
}

[[maybe_unused]] void printCodeTree([[maybe_unused]] const ZTree& codeTree) {
#ifdef YF_DEVEL_PNG
  wprintf(L"\nCode Tree");
  for (size_t i = 0; i < codeTree.size(); i++) {
    wprintf(L"\n (%lu) ", i);
    if (codeTree[i].isLeaf)
      wprintf(L"value: %u", codeTree[i].value);
    else
      wprintf(L"next: %u/%u", codeTree[i][0], codeTree[i][1]);
  }
  wprintf(L"\n");
#endif
}

INTERNAL_NS_END
