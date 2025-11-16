

/**
 * @file SdFatContentProvider.h
 * @brief DLNA content provider backed by SdFat filesystem directory tree.
 */

#include "DLNA.h"
#include "SdFatDirectoryTree.h"

namespace tiny_dlna {

/**
 * @brief Content provider for DLNA media server using SdFat filesystem.
 *
 * This class bridges the SdFat filesystem with DLNA's content
 * browsing/searching interface. It builds an in-memory directory tree from the
 * filesystem and provides DLNA-compliant metadata for media files and folders.
 *
 * The provider supports three query types:
 * - BrowseMetadata: Return metadata for a single object
 * - BrowseChildren: List children of a directory
 * - Search: Return all files in the tree
 *
 * Usage pattern:
 * 1. Call begin() to initialize and scan the filesystem
 * 2. prepareData() is called by DLNA to set up a query
 * 3. getData() is called repeatedly to retrieve individual items
 *
 * @tparam FS Filesystem type compatible with SdFat API (e.g., SdFs, SdFat32).
 */
template <typename FS>
class SdFatContentProvider {
 public:
  /**
   * @brief Set the base URL for resource URIs.
   * @param ip IP address of the server
   * @param port Port number
   * @param basePath Base path for files (e.g., "/files")
   */
  void setBaseURL(const IPAddress& ip, int port, const char* basePath) {
    baseIP_ = ip;
    basePort_ = port;
    basePath_ = basePath ? basePath : "/";
  }
  /**
   * @brief Initializes the content provider and scans the filesystem.
   * @param fs Filesystem instance (e.g., SdFs)
   * @param rootPath Root directory path to scan (defaults to "/")
   * @return true on success
   */
  bool begin(FS& fs, const char* rootPath) {
    return directoryTree.begin(fs, rootPath);
  }

  /// Releases all resources and clears the directory tree
  void end() {
    directoryTree.end();
    resultNodes_.clear();
  }

  size_t size() { return directoryTree.size(); }

  /**
   * @brief Prepares data for a DLNA content query.
   *
   * Collects all matching nodes from the directory tree and stores them in
   * resultNodes_. Calculates pagination and total match counts based on the
   * query type and parameters.
   *
   * @param objectID Object ID to query (nullptr or "0" for root)
   * @param queryType Type of query (BrowseMetadata/BrowseChildren/Search)
   * @param filter Content filter (not currently used)
   * @param startingIndex Zero-based index of first item to return
   * @param requestedCount Number of items requested (0 = all)
   * @param sortCriteria Sort criteria (not currently used)
   * @param numberReturned Output: number of items available to return
   * @param totalMatches Output: total matching items
   * @param updateID Output: content version ID
   * @param reference User pointer
   */
  void prepareData(const char* objectID, ContentQueryType queryType,
                   const char* filter, int startingIndex, int requestedCount,
                   const char* sortCriteria, int& numberReturned,
                   int& totalMatches, int& updateID, void* reference) {
    // Store parameters in class variables
    objectID_ = objectID;
    queryType_ = queryType;
    filter_ = filter;
    startingIndex_ = startingIndex;
    requestedCount_ = requestedCount;
    sortCriteria_ = sortCriteria;
    reference_ = reference;

    // Clear previous results
    resultNodes_.clear();

    // Calculate actual values based on directory tree and query type
    updateID = 0;  // Can increment this when content changes

    if (queryType == ContentQueryType::BrowseMetadata) {
      // Metadata for a single object
      TreeNode* node =
          objectID ? findNodeByObjectID(objectID) : &directoryTree.root();
      if (node) {
        resultNodes_.push_back(node);
        totalMatches = 1;
        numberReturned = 1;
      } else {
        totalMatches = 0;
        numberReturned = 0;
      }
    } else if (queryType == ContentQueryType::BrowseChildren) {
      // Browse children of a container (directory)
      TreeNode* parentNode =
          objectID ? findNodeByObjectID(objectID) : &directoryTree.root();
      if (parentNode) {
        // Collect all children
        resultNodes_ = parentNode->children;
        totalMatches = static_cast<int>(resultNodes_.size());

        // Apply pagination
        int available = totalMatches - startingIndex;
        if (available < 0) available = 0;
        numberReturned = (requestedCount == 0)
                             ? available
                             : std::min(requestedCount, available);
      } else {
        totalMatches = 0;
        numberReturned = 0;
      }
    } else {
      // Search - return all files
      resultNodes_ = directoryTree.getAllFiles();
      totalMatches = static_cast<int>(resultNodes_.size());

      // Apply pagination
      int available = totalMatches - startingIndex;
      if (available < 0) available = 0;
      numberReturned = (requestedCount == 0)
                           ? available
                           : std::min(requestedCount, available);
    }

    DlnaLogger.log(DlnaLogLevel::Info,
                   "Number of results: Total=%d, Returned=%d", totalMatches,
                   numberReturned);
  }

  /**
   * @brief Retrieves a single media item by index.
   *
   * Called repeatedly by DLNA after prepareData() to fetch individual items.
   * Uses the cached resultNodes_ populated during prepareData().
   *
   * @param index Zero-based index relative to startingIndex
   * @param item Output: MediaItem populated with metadata
   * @param reference User-defined reference pointer
   * @return true if successful, false if index out of range
   */
  bool getData(int index, MediaItem& item, void* reference = nullptr) {
    // Calculate the actual index into resultNodes_
    int adjustedIndex = startingIndex_ + index;

    // Validate index
    if (adjustedIndex < 0 ||
        adjustedIndex >= static_cast<int>(resultNodes_.size())) {
      return false;
    }

    TreeNode* node = resultNodes_[adjustedIndex];
    if (!node) return false;

    // Populate MediaItem from TreeNode
    // Use member strings for pointer lifetime
    currentId_ = std::to_string(node->id);
    item.id = currentId_.c_str();

    // Parent ID
    if (node->parent) {
      currentParentId_ = std::to_string(node->parent->id);
      item.parentID = currentParentId_.c_str();
    } else {
      item.parentID = "0";
    }

    item.title = node->file_name.c_str();
    item.restricted = true;

    if (node->is_dir) {
      item.itemClass = MediaItemClass::Folder;
      item.resourceURI = nullptr;
      item.mimeType = nullptr;
    } else {
      // File - determine class from MIME type
      currentMime_ = node->getMime();
      if (currentMime_.empty()) currentMime_ = "application/octet-stream";
      item.mimeType = currentMime_.c_str();

      if (currentMime_.find("audio") != std::string::npos) {
        item.itemClass = MediaItemClass::Music;
      } else if (currentMime_.find("video") != std::string::npos) {
        item.itemClass = MediaItemClass::Video;
      } else if (currentMime_.find("image") != std::string::npos) {
        item.itemClass = MediaItemClass::Photo;
      } else {
        item.itemClass = MediaItemClass::Unknown;
      }

      // Resource URI: baseURL + basePath + ?ID=id
      char urlBuffer[128];
      snprintf(urlBuffer, sizeof(urlBuffer), "http://%u.%u.%u.%u:%d%s?ID=%u",
               baseIP_[0], baseIP_[1], baseIP_[2], baseIP_[3], basePort_,
               basePath_.c_str(), node->id);
      currentPath_ = urlBuffer;
      item.resourceURI = currentPath_.c_str();
    }

    item.albumArtURI = nullptr;

    return true;
  }

 protected:
  IPAddress baseIP_;
  int basePort_ = 0;
  stringPSRAM basePath_ = "/";
  SdFatDirectoryTree<FS> directoryTree;

  // Store query parameters for getData
  const char* objectID_ = nullptr;
  ContentQueryType queryType_;
  const char* filter_ = nullptr;
  int startingIndex_ = 0;
  int requestedCount_ = 0;
  const char* sortCriteria_ = nullptr;
  void* reference_ = nullptr;

  // Collected results from prepareData
  std::vector<TreeNode*, AllocatorPSRAM<TreeNode*>> resultNodes_;

  // String storage for MediaItem pointers (lifetime managed by class)
  stringPSRAM currentId_;        ///< Current item ID string
  stringPSRAM currentParentId_;  ///< Current parent ID string
  stringPSRAM currentPath_;      ///< Current resource path string
  stringPSRAM currentMime_;      ///< Current MIME type string

  /**
   * @brief Finds a tree node by its object ID string.
   * @param objectID Object ID as string (node's numeric id)
   * @return Pointer to the node, or nullptr if not found or invalid
   */
  TreeNode* findNodeByObjectID(const char* objectID) {
    if (!objectID) return nullptr;
    uint32_t id = std::stoul(objectID);
    return &directoryTree.getTreeNodeById(id);
  }
};

}  // namespace tiny_dlna
