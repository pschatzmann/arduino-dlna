#include "dlna/XMLPrinter.h"
#include "State.h"

namespace tiny_dlna {

enum class Direction { In, Out };
const char* direction_str[] = {"in", "out"};

class Argument {
  const char* name;
  Direction direction;
  State* related_state;
};

class Action {
  const char* name;
  Vector<Argument> arguments;
};

}  // namespace tiny_dlna