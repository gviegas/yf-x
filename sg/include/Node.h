//
// SG
// Node.h
//
// Copyright © 2020 Gustavo C. Viegas.
//

#ifndef YF_SG_NODE_H
#define YF_SG_NODE_H

#include <functional>
#include <vector>
#include <memory>

#include "yf/sg/Defs.h"

SG_NS_BEGIN

/// Node.
///
class Node {
 public:
  Node();
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

  /// Gets the immediate ancestor.
  ///
  Node* parent() const;

  /// Gets all immediate descendants.
  ///
  std::vector<Node*> children() const;
  size_t children(std::vector<Node*>& dst) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

SG_NS_END

#endif // YF_SG_NODE_H
