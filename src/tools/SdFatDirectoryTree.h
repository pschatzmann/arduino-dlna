/**
 * @file SdFatDirectoryTree.h
 * @brief In-memory directory tree built from SdFat recursive listings.
 *
 * This header provides SdFatDirectoryTree<FS>, a small utility that uses
 * SdFat's ls() to stream directory entries into a parser and construct a
 * navigable tree. The TreeNode type is defined in TreeNode.h.
 */
#pragma once

#include <string>
#include <vector>

#include "SdFatParser.h"
#include "TreeNode.h"

namespace tiny_dlna {

// TreeNode is now declared in TreeNode.h

/**
 * @brief Simple in-memory directory tree built using SdFat recursive directory
 * listing.
 *
 * The tree is constructed by asking SdFat to list recursively (LS_R). Each line
 * of output is parsed into a node, with indentation (two spaces per level)
 * determining the hierarchy and a trailing '/' indicating directories.
 *
 * @tparam FS Filesystem type that exposes chdir() and ls(Print*, uint8_t flags)
 * compatible with SdFat's API.
 */
template <typename FS>
class SdFatDirectoryTree {
 public:
  /**
   * @brief Initializes the tree by scanning from a starting path.
   * @param SD Filesystem instance (e.g., SdFs).
   * @param beginPath Root path to start scanning from (defaults to "/").
   * @return true on success.
   */

  /// Starts the processing
  bool begin(FS& SD, const char* beginPath = "/") {
    this->p_sd = &SD;
    SD.chdir(beginPath);
    root_node.file_name = beginPath;
    buildTree(root_node);
    return true;
  }

  /**
   * @brief Releases all dynamically allocated nodes and clears the tree.
   */
  void end() {
    for (auto& ch : tree_nodes) {
      delete ch;
    }
    tree_nodes.clear();
  }

  /**
   * @brief Returns the root node of the tree.
   */
  TreeNode& root() { return root_node; }

  /**
   * @brief Finds a node by its numeric id.
   * @param id The node id assigned during tree construction.
   * @return Reference to the located TreeNode.
   */
  TreeNode& getTreeNodeById(uint32_t id) {
    TreeNode& result = *(tree_nodes[id]);
    return result;
  }

  /**
   * @brief Collects all file nodes (non-directories) under the root.
   * @return Vector with pointers to file nodes.
   */
  std::vector<TreeNode*, AllocatorPSRAM<TreeNode*>> getAllFiles() {
    std::vector<TreeNode*, AllocatorPSRAM<TreeNode*>> result;
    collectAllFilesFrom(root_node, result);
    return result;
  }

  /**
   * @brief Convenience helper to get a path by node id.
   */
  stringPSRAM path(uint32_t id) { return getTreeNodeById(id).path(); }

  /// Returns the total number of nodes in the tree
  size_t size() { return tree_nodes.size(); }

 protected:
  TreeNode root_node;
  SdFatParser parser;
  FS* p_sd = nullptr;
  std::vector<TreeNode*, AllocatorPSRAM<TreeNode*>> tree_nodes;

  // Temporary build-time state
  std::vector<TreeNode*>
      parents_;  // parents_[level] holds parent at that level

  // Recursively collect all file nodes from a tree
  void collectAllFilesFrom(
      TreeNode& node,
      std::vector<TreeNode*, AllocatorPSRAM<TreeNode*>>& result) {
    if (!node.is_dir) {
      result.push_back(&node);
    }
    for (auto* child : node.children) {
      if (child) collectAllFilesFrom(*child, result);
    }
  }

  // Static callback invoked by SdFatParser when a line is parsed
  static void onParsedCallback(SdFatFileInfo& info, void* ref) {
    auto* self = static_cast<SdFatDirectoryTree*>(ref);
    if (!self) return;

    TreeNode* root = &self->root_node;

    // Ensure parents_ has at least the root at level 0
    if (self->parents_.empty()) self->parents_.push_back(root);

    // Clamp/resize parents_ to cover current level
    if (info.level >= static_cast<int>(self->parents_.size())) {
      self->parents_.resize(static_cast<size_t>(info.level) + 1, nullptr);
    }

    TreeNode* parent = nullptr;
    if (info.level == 0) {
      parent = root;
    } else {
      parent = self->parents_[static_cast<size_t>(info.level)];
      if (parent == nullptr) parent = root;  // fallback safety
    }

    // Create and populate the node
    auto* node = new TreeNode();
    assert(node != nullptr);
    node->is_dir = info.is_directory;
    node->file_name = info.name;
    // Strip trailing '/' for directory names to keep path() consistent
    if (node->is_dir && !node->file_name.empty() &&
        node->file_name.back() == '/') {
      node->file_name.pop_back();
    }
    node->parent = parent;
    node->id = static_cast<uint32_t>(self->tree_nodes.size());

    // Link into tree
    parent->children.push_back(node);
    self->tree_nodes.push_back(node);

    // Set the parent for the next deeper level when this is a directory
    if (node->is_dir) {
      if (self->parents_.size() <= static_cast<size_t>(info.level + 1)) {
        self->parents_.resize(static_cast<size_t>(info.level + 2), nullptr);
      }
      self->parents_[static_cast<size_t>(info.level + 1)] = node;
    }
  }

  void buildTree(TreeNode& node) {
    // Reset any previous children of the root
    node.children.clear();
    tree_nodes.clear();

    // Prepare parsing context on this instance
    parents_.clear();
    parents_.push_back(&root_node);  // level 0

    parser.setCallback(&SdFatDirectoryTree::onParsedCallback, this);

    // Ask SdFat to list recursively; output is fed into our parser (Print)
    p_sd->ls(&parser, LS_R);
  }
};

}  // namespace tiny_dlna