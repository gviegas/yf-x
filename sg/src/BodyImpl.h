//
// SG
// BodyImpl.h
//
// Copyright © 2021 Gustavo C. Viegas.
//

#ifndef YF_SG_BODYIMPL_H
#define YF_SG_BODYIMPL_H

#include <forward_list>

#include "Body.h"

SG_NS_BEGIN

/// Physics Body implementation details.
///
class Body::Impl {
 public:
  Impl(const Shape& shape);
  Impl(const std::vector<Shape*>& shapes);
  Impl(const Impl& other);
  Impl& operator=(const Impl& other);

  void setNode(Node* node);
  void setPhysicsWorld(PhysicsWorld* world);

  /// Checks whether two physics bodies intersect each other.
  /// This check ignores interaction masks.
  ///
  bool intersect(Impl& other);

  [[deprecated]] static void processCollisions(const std::vector<Body*>&);

 private:
  std::vector<Sphere> spheres_{};
  std::vector<BBox> bboxes_{};
  ContactFn contactBegin_{};
  ContactFn contactEnd_{};
  bool dynamic_ = false;
  float mass_ = 1.0f;
  float restitution_ = 0.5f;
  float friction_ = 0.25f;
  PhysicsFlags categoryMask_ = 1;
  PhysicsFlags contactMask_ = 0;
  PhysicsFlags collisionMask_ = ~static_cast<PhysicsFlags>(0);
  Node* node_ = nullptr;
  Vec3f localT_{};
  PhysicsWorld* physicsWorld_ = nullptr;
  std::forward_list<Body*> contacts_{};

  void pushShape(const Shape&);
  void nextStep();
  void undoStep();

  friend Body;
};

SG_NS_END

#endif // YF_SG_BODYIMPL_H
