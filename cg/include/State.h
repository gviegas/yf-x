//
// CG
// State.h
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#ifndef YF_CG_STATE_H
#define YF_CG_STATE_H

#include <cstdint>
#include <memory>
#include <vector>

#include "yf/cg/Defs.h"

CG_NS_BEGIN

using VxId = uint32_t;

/// Vertex formats.
///
enum VxFormat {
  VxFormatByte,
  VxFormatByte2,
  VxFormatByte3,
  VxFormatByte4,

  VxFormatByteNorm,
  VxFormatByteNorm2,
  VxFormatByteNorm3,
  VxFormatByteNorm4,

  VxFormatUbyte,
  VxFormatUbyte2,
  VxFormatUbyte3,
  VxFormatUbyte4,

  VxFormatUbyteNorm,
  VxFormatUbyteNorm2,
  VxFormatUbyteNorm3,
  VxFormatUbyteNorm4,

  VxFormatShrt,
  VxFormatShrt2,
  VxFormatShrt3,
  VxFormatShrt4,

  VxFormatShrtNorm,
  VxFormatShrtNorm2,
  VxFormatShrtNorm3,
  VxFormatShrtNorm4,

  VxFormatUshrt,
  VxFormatUshrt2,
  VxFormatUshrt3,
  VxFormatUshrt4,

  VxFormatUshrtNorm,
  VxFormatUshrtNorm2,
  VxFormatUshrtNorm3,
  VxFormatUshrtNorm4,

  VxFormatInt,
  VxFormatInt2,
  VxFormatInt3,
  VxFormatInt4,

  VxFormatInt2x2,
  VxFormatInt2x3,
  VxFormatInt2x4,

  VxFormatInt3x2,
  VxFormatInt3x3,
  VxFormatInt3x4,

  VxFormatInt4x2,
  VxFormatInt4x3,
  VxFormatInt4x4,

  VxFormatUint,
  VxFormatUint2,
  VxFormatUint3,
  VxFormatUint4,

  VxFormatUint2x2,
  VxFormatUint2x3,
  VxFormatUint2x4,

  VxFormatUint3x2,
  VxFormatUint3x3,
  VxFormatUint3x4,

  VxFormatUint4x2,
  VxFormatUint4x3,
  VxFormatUint4x4,

  VxFormatFlt,
  VxFormatFlt2,
  VxFormatFlt3,
  VxFormatFlt4,

  VxFormatFlt2x2,
  VxFormatFlt2x3,
  VxFormatFlt2x4,

  VxFormatFlt3x2,
  VxFormatFlt3x3,
  VxFormatFlt3x4,

  VxFormatFlt4x2,
  VxFormatFlt4x3,
  VxFormatFlt4x4,

  VxFormatDbl,
  VxFormatDbl2,
  VxFormatDbl3,
  VxFormatDbl4,

  VxFormatDbl2x2,
  VxFormatDbl2x3,
  VxFormatDbl2x4,

  VxFormatDbl3x2,
  VxFormatDbl3x3,
  VxFormatDbl3x4,

  VxFormatDbl4x2,
  VxFormatDbl4x3,
  VxFormatDbl4x4
};

/// Vertex attribute.
///
struct VxAttr {
  VxId id;
  VxFormat format;
  uint32_t offset;
};

/// Vertex step function.
///
enum VxStepFn {
  VxStepFnVertex,
  VxStepFnInstance
};

/// Collection of vertex attributes using the same buffer binding.
///
struct VxInput {
  std::vector<VxAttr> attributes;
  uint32_t stride;
  VxStepFn stepFunction;
};

/// Primitive topologies.
///
enum Topology {
  TopologyPoint,
  TopologyLine,
  TopologyTriangle,
  TopologyLnStrip,
  TopologyTriStrip,
  TopologyTriFan
};

/// Polygon modes.
///
enum PolyMode {
  PolyModeFill,
  PolyModeLine,
  PolyModePoint
};

/// Cull modes.
///
enum CullMode {
  CullModeNone,
  CullModeFront,
  CullModeBack,
  CullModeAny
};

/// Directions to determine front/back-facing primitives.
///
enum Winding {
  WindingClockwise,
  WindingCounterCw
};

class Pass;
class Shader;
class DcTable;

/// Graphics state.
///
class GrState {
 public:
  using Ptr = std::unique_ptr<GrState>;

  /// Configuration that determines how the state operates.
  ///
  struct Config {
    Pass* pass;
    std::vector<Shader*> shaders;
    std::vector<DcTable*> dcTables;
    std::vector<VxInput> vxInputs;
    Topology topology;
    PolyMode polyMode;
    CullMode cullMode;
    Winding winding;
  };

  GrState() = default;
  GrState(const GrState&) = delete;
  GrState& operator=(const GrState&) = delete;
  virtual ~GrState() = default;

  /// Gets the state's configuration.
  ///
  virtual const Config& config() const = 0;
};

/// Compute state.
///
class CpState {
 public:
  using Ptr = std::unique_ptr<CpState>;

  /// Configuration that determines how the state operates.
  ///
  struct Config {
    Shader* shader;
    std::vector<DcTable*> dcTables;
  };

  CpState() = default;
  CpState(const CpState&) = delete;
  CpState& operator=(const CpState&) = delete;
  virtual ~CpState() = default;

  /// Gets the state's configuration.
  ///
  virtual const Config& config() const = 0;
};

CG_NS_END

#endif // YF_CG_STATE_H
