// Ensure correct Print base class from ArduinoCore-API
#include "Print.h"
#pragma once

namespace tiny_dlna {

/**
 * @brief Subscription State for DLNA eventing
 *
 * Used to track the state of a subscription to a DLNA service event.
 */
enum class SubscriptionState {
  Unsubscribed, /**< Not subscribed to the event */
  Subscribing,  /**< Currently subscribing to the event */
  Subscribed,   /**< Successfully subscribed to the event */
  Unsubscribing /**< Currently unsubscribing from the event */
};

/**
 * @brief Role to indicate whether a protocolInfo entry is a Source or a Sink
 *
 * Used to distinguish between source and sink roles in DLNA protocolInfo
 * entries.
 */
enum class ProtocolRole {
  IsSource = 0, /**< Entry is a source */
  IsSink = 1    /**< Entry is a sink */
};

/**
 * @brief Type of content query for DLNA browsing/searching
 *
 * Used to specify the type of query performed on DLNA content directories.
 */
enum class ContentQueryType {
  Search,         /**< Search for content */
  BrowseMetadata, /**< Browse metadata of a content item */
  BrowseChildren  /**< Browse children of a container */
};

class NullPrint : public Print {
 public:
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t size) override { return size; }
};

}  // namespace tiny_dlna