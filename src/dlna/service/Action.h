#include "State.h"
#include "dlna/XMLPrinter.h"

namespace tiny_dlna {

enum class Direction { In, Out };
const char* direction_str[] = {"in", "out"};

/**
 * @brief DLNA Service: Action Argument
 * @author Phil Schatzmann
 */
class Argument {
  const char* name;
  Direction direction;
  State* related_state;
};

/**
 * @brief DLNA Service: Action
 * @author Phil Schatzmann
 */
class Action {
  const char* name;
  Vector<Argument> arguments;
};

}  // namespace tiny_dlna