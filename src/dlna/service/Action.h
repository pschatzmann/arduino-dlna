#pragma once
#include "State.h"
#include "dlna/DLNAServiceInfo.h"
#include "dlna/xml/XMLPrinter.h"

namespace tiny_dlna {

// enum class Direction { In, Out };
// const char* direction_str[] = {"in", "out"};

/**
 * @brief DLNA Service: Action Argument
 * @author Phil Schatzmann
 */
class Argument {
 public:
  Argument() = default;
  Argument(const char* nme, const char* val) {
    name = nme;
    value = val;
  }
  const char* name;
  // Direction direction;
  // StateInformation* related_state;
  Str value;
};

// /**
//  * @brief DLNA Service: Action
//  * @author Phil Schatzmann
//  */
// class Action {
//   const char* name;
//   Vector<Argument> in;
//   Vector<Argument> out;
// };

class ActionReply {
 public:
  ActionReply(bool valid = true) { is_valid = valid; }
  Vector<Argument> arguments;
  operator bool() { return is_valid; }
  bool isValid() const { return is_valid; }
  void setValid(bool flag) { is_valid = flag; }
  void add(ActionReply alt) {
    if (!alt) setValid(false);
    for (auto& arg : alt.arguments) {
      arguments.push_back(arg);
    }
  }

 protected:
  bool is_valid = true;
};

class ActionRequest {
 public:
  ActionRequest() = default;

  ActionRequest(DLNAServiceInfo& srv, const char* act) {
    p_service = &srv;
    action = act;
  }

  void addArgument(Argument arg) { arguments.push_back(arg); }
  void addArgument(const char* name, const char* value) {
    Argument arg{name, value};
    addArgument(arg);
  }

  DLNAServiceInfo* p_service = nullptr;
  const char* action;
  Vector<Argument> arguments;
  int result_count = 0;
  operator bool() { return is_valid; }
  const char* getServiceType() { return p_service->service_type; }

 protected:
  bool is_valid = false;
};

}  // namespace tiny_dlna