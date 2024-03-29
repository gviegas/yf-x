//
// SG
// Physics.cxx
//
// Copyright © 2021 Gustavo C. Viegas.
//

#include <type_traits>
#include <algorithm>
#include <cassert>

#include "PhysicsImpl.h"
#include "BodyImpl.h"
#include "Node.h"

using namespace SG_NS;
using namespace std;

PhysicsWorld::PhysicsWorld() : impl_(make_unique<Impl>(*this)) { }

PhysicsWorld::PhysicsWorld(const PhysicsWorld& other)
  : impl_(make_unique<Impl>(*this, *other.impl_)) { }

PhysicsWorld& PhysicsWorld::operator=(const PhysicsWorld& other) {
  impl_ = make_unique<Impl>(*this, *other.impl_);
  return *this;
}

PhysicsWorld::~PhysicsWorld() { }

Vec3f& PhysicsWorld::gravity() {
  return impl_->gravity_;
}

const Vec3f& PhysicsWorld::gravity() const {
  return impl_->gravity_;
}

void PhysicsWorld::enable() {
  impl_->enabled_ = true;
}

void PhysicsWorld::disable() {
  impl_->enabled_ = false;
}

bool PhysicsWorld::isEnabled() const {
  return impl_->enabled_;
}

PhysicsWorld::Impl& PhysicsWorld::impl() {
  return *impl_;
}

PhysicsWorld::Impl::Impl(PhysicsWorld& physicsWorld)
  : physicsWorld_(physicsWorld) { }

PhysicsWorld::Impl::Impl(PhysicsWorld& physicsWorld, const Impl& other)
  : physicsWorld_(physicsWorld), enabled_(other.enabled_) { }

void PhysicsWorld::Impl::add(Body& body) {
  assert(find(bodies_.begin(), bodies_.end(), &body) == bodies_.end() ||
         pendingChanges_.find(&body) != pendingChanges_.end());

  auto res = pendingChanges_.insert(&body);
  if (!res.second)
    pendingChanges_.erase(res.first);
}

void PhysicsWorld::Impl::remove(Body& body) {
  assert(find(bodies_.begin(), bodies_.end(), &body) != bodies_.end() ||
         pendingChanges_.find(&body) != pendingChanges_.end());

  auto res = pendingChanges_.insert(&body);
  if (!res.second)
    pendingChanges_.erase(res.first);
}

void PhysicsWorld::Impl::update(Body& body, PhysicsFlags prevCategoryMask) {
  assert(find(bodies_.begin(), bodies_.end(), &body) != bodies_.end());

  auto res = pendingUpdates_.insert({&body, prevCategoryMask});
  if (!res.second && res.first->second == body.categoryMask())
    pendingUpdates_.erase(res.first);
}

void PhysicsWorld::Impl::clear() {
  for (auto& body : bodies_)
    body->impl().setPhysicsWorld(nullptr);
  bodies_.clear();

  for (auto& group : groups_)
    group.clear();

  for (auto& body : pendingChanges_)
    if (!body->node())
      // Was released by its Node - get rid of it
      delete body;
  pendingChanges_.clear();

  pendingUpdates_.clear();
}

void PhysicsWorld::Impl::evaluate(chrono::nanoseconds) {
  print();

  // XXX: Changes must be applied first
  applyChanges();
  applyUpdates();

  print();

  if (!enabled_)
    return;

  // TODO: Cache results to avoid redundant intersection checks
  for (const auto& body : bodies_) {
    auto contactMask = body->contactMask();
    auto collisionMask = body->collisionMask();
    auto mask = contactMask | collisionMask;
    uint32_t i = 0;
    auto& impl = body->impl();
    static_assert(!is_signed<decltype(mask)>());

    while (mask != 0) {
      if (contactMask & mask & 1) {
        if (collisionMask & mask & 1) {
          // Contact and collision
          for (const auto& other : groups_[i]) {
            const bool intersect = impl.intersect(*other);
            impl.updateContact(*body, *other, intersect);
            impl.updateCollision(*other, intersect);
          }
        } else {
          // Contact only
          for (const auto& other : groups_[i])
            impl.updateContact(*body, *other, impl.intersect(*other));
        }
      } else if (collisionMask & mask & 1) {
        // Collision only
        for (const auto& other : groups_[i])
          impl.updateCollision(*other, impl.intersect(*other));
      }
      contactMask >>= 1;
      collisionMask >>= 1;
      mask >>= 1;
      i++;
    }
  }

  for (const auto& body : bodies_)
    body->impl().resolveInteractions(*body);
}

void PhysicsWorld::Impl::applyChanges() {
  if (pendingChanges_.empty())
    return;

  auto changesIt = pendingChanges_.begin();
  auto bodiesIt = bodies_.begin();
  array<list<Body*>::iterator, CategoryN> groupsIts{};

  for (uint32_t i = 0; i < CategoryN; i++)
    groupsIts[i] = groups_[i].begin();

  // Remove `*changesIt` from physics world
  auto remove = [&] {
    bodiesIt = bodies_.erase(bodiesIt);
    const auto body = *changesIt++;

    PhysicsFlags categoryMask;
    uint32_t i = 0;
    static_assert(!is_signed<PhysicsFlags>());

    // Category mask might have changed before removal
    const auto updateIt = pendingUpdates_.find(body);
    if (updateIt != pendingUpdates_.end()) {
      categoryMask = updateIt->second;
      pendingUpdates_.erase(updateIt);
    } else {
      categoryMask = body->categoryMask();
    }

    for (; categoryMask != 0; categoryMask >>= 1, i++) {
      if (categoryMask & 1) {
        while (*groupsIts[i] != body)
          groupsIts[i]++;
        groupsIts[i] = groups_[i].erase(groupsIts[i]);
      }
    }

    if (!body->node())
      // Was released by its Node - get rid of it
      delete body;
    else
      body->impl().setPhysicsWorld(nullptr);
  };

  // Add `*changesIt` to physics world
  auto add = [&] {
    bodies_.insert(bodiesIt, *changesIt);
    const auto body = *changesIt++;

    auto categoryMask = body->categoryMask();
    uint32_t i = 0;
    static_assert(!is_signed<decltype(categoryMask)>());

    for (; categoryMask != 0; categoryMask >>= 1, i++) {
      if (categoryMask & 1) {
        while (groupsIts[i] != groups_[i].end() && *groupsIts[i] < body)
          groupsIts[i]++;
        groups_[i].insert(groupsIts[i], body)++;
      }
    }

    body->impl().setPhysicsWorld(&physicsWorld_);
    // Updating this body is redundant
    pendingUpdates_.erase(body);
  };

  while (bodiesIt != bodies_.end()) {
    if (*changesIt > *bodiesIt) {
      bodiesIt++;
    } else {
      *changesIt == *bodiesIt ? remove() : add();
      if (changesIt == pendingChanges_.end()) {
        pendingChanges_.clear();
        return;
      }
    }
  }
  while (changesIt != pendingChanges_.end())
    add();
  pendingChanges_.clear();
}

void PhysicsWorld::Impl::applyUpdates() {
  if (pendingUpdates_.empty())
    return;

  array<list<Body*>::iterator, CategoryN> groupsIts{};
  for (uint32_t i = 0; i < CategoryN; i++)
    groupsIts[i] = groups_[i].begin();

  for (const auto& kv : pendingUpdates_) {
    const auto body = kv.first;
    const auto categoryMask = body->categoryMask();
    const auto prevCategoryMask = kv.second;
    auto toInsert = categoryMask & ~prevCategoryMask;
    auto toErase = prevCategoryMask & ~categoryMask;
    uint32_t i;
    static_assert(!is_signed<decltype(toInsert)>());
    static_assert(!is_signed<decltype(toErase)>());

    for (i = 0; toInsert != 0; toInsert >>= 1, i++) {
      if (toInsert & 1) {
        while (groupsIts[i] != groups_[i].end() && *groupsIts[i] < body)
          groupsIts[i]++;
        groups_[i].insert(groupsIts[i], body)++;
      }
    }

    for (i = 0; toErase != 0; toErase >>= 1, i++) {
      if (toErase & 1) {
        while (*groupsIts[i] != body)
          groupsIts[i]++;
        groupsIts[i] = groups_[i].erase(groupsIts[i]);
      }
    }
  }

  pendingUpdates_.clear();
}

//
// DEVEL
//

void PhysicsWorld::Impl::print() const {
#ifdef YF_DEVEL_PHYSICSWORLD
  wprintf(L"\nPhysicsWorld\n");

  auto printBody = [](Body* body, const char* indent) {
    wprintf(L"%sphysics body %p:\n"
            L"%s node: %p (%ls)\n"
            L"%s category mask: %Xh\n",
            indent, static_cast<void*>(body),
            indent, static_cast<void*>(body->node()),
            (body->node() ? body->node()->name().data() : L""),
            indent, body->categoryMask());
  };

  wprintf(L" physics bodies: #%zu\n", bodies_.size());
  for (const auto& body : bodies_)
    printBody(body, "  ");

  wprintf(L" physics groups: #%zu\n", groups_.size());
  uint32_t grpIndex = 0;
  for (const auto& group : groups_) {
    wprintf(L"  [%u]: #%zu\n", grpIndex, groups_[grpIndex].size());
    grpIndex++;
    for (const auto& body : group)
      printBody(body, "   ");
  }

  wprintf(L" pending changes: #%zu\n", pendingChanges_.size());
  for (const auto& body : pendingChanges_)
    printBody(body, "  ");

  wprintf(L" pending updates: #%zu\n", pendingUpdates_.size());
  for (const auto& kv : pendingUpdates_) {
    printBody(kv.first, "  ");
    wprintf(L"   prev. category mask: %Xh\n", kv.second);
  }
#endif
}
