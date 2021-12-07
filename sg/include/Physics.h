//
// SG
// Physics.h
//
// Copyright © 2021 Gustavo C. Viegas.
//

#ifndef YF_SG_PHYSICS_H
#define YF_SG_PHYSICS_H

#include <cstdint>
#include <memory>

#include "yf/sg/Defs.h"

SG_NS_BEGIN

/// Type used when defining physics interactions.
///
using PhysicsFlags = uint32_t;

/// Physics world.
///
class PhysicsWorld {
 public:
  /// Enable/disable physics simulation.
  ///
  void enable();
  void disable();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  PhysicsWorld();
  PhysicsWorld(const PhysicsWorld& other);
  PhysicsWorld& operator=(const PhysicsWorld& other);
  ~PhysicsWorld();

  friend class Scene;
  friend class View;
};

SG_NS_END

#endif // YF_SG_PHYSICS_H
