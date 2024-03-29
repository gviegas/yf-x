//
// SG
// Node.h
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#ifndef YF_SG_NODE_H
#define YF_SG_NODE_H

#include <cstddef>
#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "yf/sg/Defs.h"
#include "yf/sg/Matrix.h"
#include "yf/sg/Quaternion.h"
#include "yf/sg/Body.h"

SG_NS_BEGIN

/// Node.
///
class Node {
 public:
  using Ptr = std::unique_ptr<Node>;

  Node();
  Node(const Node& other);
  Node& operator=(const Node& other);
  virtual ~Node();

  /// Inserts descendant node(s).
  ///
  void insert(Node& child);
  void insert(const std::vector<Node*>& children);

  /// Removes itself from immediate ancestor.
  ///
  void drop();

  /// Removes all immediate descendants.
  ///
  void prune();

  /// Traverses the node graph.
  ///
  void traverse(const std::function<bool (Node&)>& callback, bool ignoreSelf);

  /// Traverses the node graph, unconditionally.
  ///
  void traverse(const std::function<void (Node&)>& callback, bool ignoreSelf);

  /// Counts the number of nodes in the graph.
  ///
  size_t count() const;

  /// Checks whether a node descends from another.
  ///
  bool isDescendantOf(const Node& node) const;

  /// Checks whether a node has no descendants.
  ///
  bool isLeaf() const;

  /// Checks whether a node has no ancestors.
  ///
  bool isRoot() const;

  /// Checks whether a node can have ancestors.
  ///
  virtual bool isInsertable() const;

  /// Gets the immediate ancestor.
  ///
  Node* parent();
  const Node* parent() const;

  /// Gets all immediate descendants.
  ///
  std::vector<Node*> children() const;
  size_t children(std::vector<Node*>& dst) const;

  /// Gets the root of the graph containing the node.
  ///
  Node* root();
  const Node* root() const;

  /// Gets the node's name.
  ///
  std::wstring& name();
  const std::wstring& name() const;

  /// Gets the node's transform.
  ///
  Mat4f& transform();
  const Mat4f& transform() const;

  /// Sets the node's TRS properties.
  ///
  /// XXX: When any of the TRS properties are set, the node's `transform()`
  /// is overwritten.
  ///
  void setT(const Vec3f& t);
  void setR(const Qnionf& r);
  void setS(const Vec3f& s);

  /// Gets the node's world transform.
  ///
  Mat4f& worldTransform();
  const Mat4f& worldTransform() const;

  /// Gets the node's inverse world transform.
  ///
  Mat4f& worldInverse();
  const Mat4f& worldInverse() const;

  /// Gets the node's normal matrix.
  ///
  Mat4f& worldNormal();
  const Mat4f& worldNormal() const;

  /// Gets/sets the node's physics body.
  ///
  void setBody(Body::Ptr&& body);
  void setBody(const Body& body);
  Body* body();

 protected:
  /// Notifies the node and its direct ancestors of an insert() call.
  ///
  virtual void willInsert(Node& node);

  /// Notifies the node and its direct ancestors of a drop() call.
  ///
  virtual void willDrop(Node& node);

  /// Notifies the node and its direct ancestors of a prune() call.
  ///
  virtual void willPrune(Node& node);

  /// Notifies the node and its direct ancestors of a setBody() call.
  ///
  virtual void willSetBody(Node& node, Body* body);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

SG_NS_END

#endif // YF_SG_NODE_H
