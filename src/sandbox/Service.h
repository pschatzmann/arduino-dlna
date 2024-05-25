#pragma once
#include "basic/Vector.h"
#include "Action.h"
#include "State.h"
#include "dlna/xml/XMLPrinter.h"

namespace tiny_dlna {

/**
 * @brief DLNA Service: Actions and State information
 * @author Phil Schatzmann
*/

class Service {
 public:
  Vector<Action> actions;
  Vector<State> state;

  // int printXML(Print &out) {
  //   int len = 0;
  //   for (auto &action : actions) len += action.printXML(out);

  //   for (auto &st : state) len += st.printXML(out);

  //   return len;
  // }
};

}  // namespace tiny_dlna