#pragma once
#include "dlna/DLNAServiceInfo.h"
#include "dlna/xml/XMLPrinter.h"

namespace tiny_dlna {

  /// Role to indicate whether a protocolInfo entry is a Source or a Sink
enum class ProtocolRole { IsSource = 0, IsSink = 1 };


/// Search or Browse
enum class ContentQueryType { Search, BrowseMetadata, BrowseChildren };

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
  const char* name = nullptr;
  Str value = "";
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
      if (StrView(a.name).equals(arg.name)) {
        a.value = arg.value;
        return;
      }
    }
    arguments.push_back(arg);
  }

  // Helper: find argument by name in ActionReply (returns nullptr if not found)
  const char* findArgument(const char* name) {
    for (auto& a : arguments) {
      if (a.name != nullptr) {
        StrView nm(a.name);
        if (nm == name) return a.value.c_str();
      }
    }
    return nullptr;
  }

  operator bool() { return is_valid; }

  int size() { return arguments.size(); }

  void clear() { arguments.clear(); }

  void logArguments(){
    for (auto& a : arguments) {
      DlnaLogger.log(DlnaLogLevel::Info, "  -> %s = %s", nullStr(a.name),
                     nullStr(a.value.c_str()));
    }
  }

 protected:
  Vector<Argument> arguments;
  bool is_valid = true;
  const char* nullStr(const char* str, const char* alt = "") {
    return str != nullptr ? str : alt;
  }
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
      if (a.name != nullptr) {
        StrView nm(a.name);
        if (nm.endsWithIgnoreCase(name)) {
          return nullStr(a.value.c_str());
        }
      }
    }

    Str list{80};
    for (auto& a : arguments) {
      list.add(a.name);
      list.add(" ");
    }

    DlnaLogger.log(DlnaLogLevel::Info, "Argument '%s' not found in (%s)", name,
                   list.c_str());
    return "";
  }

  int getArgumentIntValue(const char* name) {
    return StrView(getArgumentValue(name)).toInt();
  }

  DLNAServiceInfo* p_service = nullptr;
  const char* action = nullptr;
  Vector<Argument> arguments{10};
  int result_count = 0;
  operator bool() {
    return p_service != nullptr && action != nullptr &&
           p_service->service_type != nullptr;
  }
  const char* getServiceType() { return p_service->service_type; }
  const char* nullStr(const char* str, const char* alt = "") {
    return str != nullptr ? str : alt;
  }
};

}  // namespace tiny_dlna