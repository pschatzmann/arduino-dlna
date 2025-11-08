
#pragma once
// #include "dlna/DLNAServiceInfo.h"
#include "dlna/DLNACommon.h"
#include "dlna/xml/XMLPrinter.h"
#include "basic/Logger.h"  // ensure DlnaLogger and DlnaLogLevel declarations

namespace tiny_dlna {

class DLNAServiceInfo;
class ActionRequest;

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
  Str name{20};
  Str value{0};
};

/**
 * @brief Represents the result of invoking a DLNA service Action.
 *
 * An ActionReply collects the returned output arguments of a successful
 * action invocation and carries a validity flag indicating whether the
 * action succeeded. Callers may inspect the reply using the boolean
 * operator or the `isValid()` accessor. Returned output arguments are
 * stored in the public `arguments` vector as `Argument` entries.
 *
 * Usage:
 * - Construct an ActionReply (defaults to valid=true).
 * - Inspect success with `if (reply)` or `reply.isValid()`.
 * - Access returned arguments via `reply.arguments`.
 * - Use `add()` to merge another ActionReply (useful when aggregating
 *   results); if the merged reply is invalid the receiver is marked
 *   invalid as well.
 *
 * Note: The class is intentionally small and POD-like to keep embedded
 * footprints minimal.
 */
class ActionReply {
 public:
  ActionReply(bool valid = true) { is_valid = valid; }
  bool isValid() const { return is_valid; }
  void setValid(bool flag) { is_valid = flag; }
  void add(ActionReply alt) {
    if (!alt) setValid(false);
    for (auto& arg : alt.arguments) {
      arguments.push_back(arg);
    }
  }
  void addArgument(Argument arg) {
    for (auto& a : arguments) {
      if (StrView(a.name).equals(arg.name.c_str())) {
        a.value = arg.value;
        return;
      }
    }
    arguments.push_back(arg);
  }

  // Helper: find argument by name in ActionReply (returns nullptr if not found)
  const char* findArgument(const char* name) {
    for (auto& a : arguments) {
      if (a.name.equals(name)) return a.value.c_str();
    }
    return nullptr;
  }

  operator bool() { return is_valid; }

  int size() { return arguments.size(); }

  void clear() { arguments.clear(); }

  void logArguments() {
    for (auto& a : arguments) {
      DlnaLogger.log(DlnaLogLevel::Debug, "  -> %s = %s",
                     StrView(a.name.c_str()).c_str(),
                     StrView(a.value.c_str()).c_str());
    }
  }

 protected:
  Vector<Argument> arguments;
  bool is_valid = true;
};

/**
 * @brief Represents a request to invoke a remote DLNA service action.
 *
 * An ActionRequest contains a pointer to the target `DLNAServiceInfo`, the
 * action name to call and a list of input `Argument` items. It is used by
 * the control point to serialize and post a SOAP request to the service's
 * control URL.
 */
class ActionRequest {
 public:
  ActionRequest() = default;

  ActionRequest(DLNAServiceInfo& srv, const char* act) {
    p_service = &srv;
    action = act;
  }

  void addArgument(Argument arg) { arguments.push_back(arg); }

  void addArgument(const char* name, const char* value) {
    if (!StrView(value).isEmpty()) {
      for (auto& a : arguments) {
        if (StrView(a.name).equals(name)) {
          a.value = value;
          return;
        }
      }
      Argument arg{name, value};
      addArgument(arg);
    }
  }

  const char* getArgumentValue(const char* name) {
    for (auto& a : arguments) {
      if (a.name.endsWithIgnoreCase(name)) {
  return StrView(a.value.c_str()).c_str();
      }
    }

    Str list{80};
    for (auto& a : arguments) {
      list.add(a.name.c_str());
      list.add(" ");
    }

    DlnaLogger.log(DlnaLogLevel::Info, "Argument '%s' not found in (%s)", name,
                   list.c_str());
    return "";
  }

  int getArgumentIntValue(const char* name) {
    return StrView(getArgumentValue(name)).toInt();
  }

  void clear() {
    arguments.clear();
    action.clear();
  }

  void setAction(const char* act) { action = act; }

  const char* getAction() { return action.c_str(); }

  Str& getActionStr() { return action; }

  void setService(DLNAServiceInfo* srv) { p_service = srv; }

  DLNAServiceInfo* getService() { return p_service; }

  Vector<Argument>& getArguments() { return arguments; }

  const Vector<Argument>& getArguments() const { return arguments; }

  void setResultCount(int v) { result_count = v; }

  int getResultCount() const { return result_count; }

  operator bool() { return p_service != nullptr && !action.isEmpty(); }

 protected:
  DLNAServiceInfo* p_service = nullptr;
  // Start with an empty vector; previously we default-constructed with a
  // size of 10 which introduced 10 empty arguments and led to malformed
  // SOAP XML (< /> repeated). We only store explicitly added arguments.
  Vector<Argument> arguments;  
  int result_count = 0;
  Str action;
};

}  // namespace tiny_dlna