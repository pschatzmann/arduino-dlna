#include "basic/Vector.h"
#include "dlna/XMLPrinter.h"

namespace tiny_dlna {

enum class DataType { boolean, string, ui2, i2 };
const char* datatype_str[] = {"boolean", "string", "ui2", "u2"};

/**
 * @brief DLNA Service: Representation of a single State
 * @author Phil Schatzmann
*/

class State {
 public:
  const char* name;
  bool send_events = false;
  DataType data_type;
  bool allowed_value_range = false;
  int range_from = 0;
  int range_to = 0;
  int step = 0;
  Vector<char*> allowed_values;

  // int printXML(Print& out) { return 0; }
};

}  // namespace tiny_dlna