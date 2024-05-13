#include "basic/Str.h"

namespace tiny_dlna {

class Notification {
 public:
  uint32_t valid_until;
  Str notification_url;
};
}