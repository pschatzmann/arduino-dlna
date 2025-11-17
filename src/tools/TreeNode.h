/**
 * @file TreeNode.h
 * @brief Node type representing files and directories for the SdFat directory
 * tree.
 */
#pragma once

#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace tiny_dlna {

/**
 * @brief Node in the directory tree representing either a file or a directory.
 */
class TreeNode {
 public:
  TreeNode() = default;
  uint32_t id = 0;
  stringPSRAM file_name;
  std::vector<TreeNode*, AllocatorPSRAM<TreeNode*>> children;
  TreeNode* parent = nullptr;
  uint32_t size = 0;
  bool is_expanded = false;
  bool is_dir = false;

  static void* operator new(std::size_t size) {
    AllocatorPSRAM<TreeNode> alloc;
    return alloc.allocate(1);
  }

  static void operator delete(void* ptr) {
    AllocatorPSRAM<TreeNode> alloc;
    alloc.deallocate(static_cast<TreeNode*>(ptr), 1);
  }

  /**
   * @brief Replace all MIME rules.
   *
   * Extensions are matched case-insensitively against the file name and should
   * include the leading dot (e.g. ".mp3"). If provided without a dot or with
   * uppercase letters, they will be normalized.
   */
  static void setMimeRules(
      const std::vector<std::pair<std::string, std::string>>& rules) {
    auto& tbl = mimeRulesTable();
    tbl.clear();
    tbl.reserve(rules.size());
    for (auto& r : rules) tbl.emplace_back(normalizeExt(r.first), r.second);
  }

  /**
   * @brief Add one MIME rule mapping extension -> MIME type.
   */
  static void addMimeRule(const std::string& ext, const std::string& mime) {
    mimeRulesTable().emplace_back(normalizeExt(ext), mime);
  }

  /**
   * @brief Very basic MIME inference based on file extension.
   * @return A MIME type string for known audio formats, or empty string if
   * unknown or directory.
   */
  std::string getMime() const {
    if (is_dir) return {};
    std::string lower;
    lower.reserve(file_name.size());
    for (unsigned char c : file_name) lower.push_back((char)std::tolower(c));
    const auto& rules = mimeRulesTable();
    for (const auto& r : rules) {
      const std::string& ext = r.first;
      const std::string& mime = r.second;
      if (lower.size() >= ext.size() &&
          lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0) {
        return mime;
      }
    }
    return {};
  }

  /**
   * @brief Computes the full path from the root to this node.
   * @return Path string where directory components are separated by '/'.
   */
  std::string path() const {
    if (parent == nullptr) {
      std::string result = file_name.c_str();
      return result;
    }
    std::string parentPath = parent->path();
    if (parentPath.back() != '/') parentPath += '/';
    parentPath += file_name.c_str();
    return parentPath;
  }

  /**
   * @brief Returns the nesting level from the root.
   * @return 0 for the root, otherwise number of parents to reach the root.
   */
  int level() const {
    int lvl = 0;
    const TreeNode* p = parent;
    while (p) {
      ++lvl;
      p = p->parent;
    }
    return lvl;
  }

 private:
  // Accessor for the rules table with defaults populated once.
  static std::vector<std::pair<std::string, std::string>>& mimeRulesTable() {
    static std::vector<std::pair<std::string, std::string>> rules = {
        {".mp3", "audio/mpeg"}, {".aac", "audio/aac"},   {".m4a", "audio/aac"},
        {".wav", "audio/wav"},  {".flac", "audio/flac"}, {".ogg", "audio/ogg"},
    };
    return rules;
  }

  static std::string normalizeExt(std::string ext) {
    if (ext.empty()) return ext;
    // ensure leading dot
    if (ext[0] != '.') ext.insert(ext.begin(), '.');
    // lowercase
    for (auto& ch : ext) ch = (char)std::tolower((unsigned char)ch);
    return ext;
  }
};

}  // namespace tiny_dlna
