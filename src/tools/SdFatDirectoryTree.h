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
  bool begin(FS& SD, const char* beginPath = "/") {
    this->p_sd = &SD;
    SD.chdir(beginPath);
    // Allocate root node as first entry in tree_nodes
    tree_nodes.clear();
    auto* root = new TreeNode();
    root->file_name = beginPath;
    root->parent = nullptr;
    root->id = 0;
    tree_nodes.push_back(root);
    buildTree(*root);
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
  TreeNode& root() { return *tree_nodes[0]; }

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
    if (!tree_nodes.empty()) collectAllFilesFrom(*tree_nodes[0], result);
    return result;
  }

  /**
   * @brief Convenience helper to get a path by node id.
   */
  stringPSRAM path(uint32_t id) { return getTreeNodeById(id).path(); }

  /// Returns the total number of nodes in the tree
  size_t size() { return tree_nodes.size(); }

 protected:
  SdFatParser parser;
  FS* p_sd = nullptr;
  std::vector<TreeNode*, AllocatorPSRAM<TreeNode*>> tree_nodes;
  std::vector<TreeNode*> parent_stack;  // stack for parent management

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
    TreeNode* root = self->tree_nodes[0];
    // Initialize stack with root if empty
    if (self->parent_stack.empty()) self->parent_stack.push_back(root);

    // Ensure parent_stack has exactly info.level+1 elements (root at 0)
    while ((int)self->parent_stack.size() > info.level + 1) {
      self->parent_stack.pop_back();
    }
    while ((int)self->parent_stack.size() < info.level + 1) {
      self->parent_stack.push_back(nullptr);
    }

    TreeNode* parent = self->parent_stack[info.level];

    auto* node = new TreeNode();
    assert(node != nullptr);
    node->is_dir = info.is_directory;
    node->file_name = info.name;
    node->parent = parent;
    node->id = static_cast<uint32_t>(self->tree_nodes.size());
    if (parent != nullptr) parent->children.push_back(node);
    self->tree_nodes.push_back(node);

    // If this is a directory, ensure parent_stack[info.level+1] is this node
    if (node->is_dir) {
      if (self->parent_stack.size() <= static_cast<size_t>(info.level + 1)) {
        self->parent_stack.resize(static_cast<size_t>(info.level + 2), nullptr);
      }
      self->parent_stack[static_cast<size_t>(info.level + 1)] = node;
    }
  }

  void buildTree(TreeNode& node) {
    // Reset any previous children of the root
    node.children.clear();
    // Do not clear tree_nodes here; root is already allocated and in
    // tree_nodes[0]

    // Prepare parsing context on this instance
    parent_stack.clear();
    parent_stack.push_back(tree_nodes[0]);  // root on stack

    parser.setCallback(&SdFatDirectoryTree::onParsedCallback, this);

    // Ask SdFat to list recursively; output is fed into our parser (Print)
    p_sd->ls(&parser, LS_R);
  }
};

}  // namespace tiny_dlna