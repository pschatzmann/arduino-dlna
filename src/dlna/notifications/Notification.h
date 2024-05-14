#include "basic/Str.h"

namespace tiny_dlna {

/**
 * @brief DLNA notification
 * @author Phil Schatzmann
*/
class Notification {
 public:
  uint32_t valid_until;
  Str notification_url;
};
}