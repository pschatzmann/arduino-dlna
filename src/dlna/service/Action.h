#pragma once
#include "dlna/DLNAServiceInfo.h"
#include "dlna/xml/XMLPrinter.h"

namespace tiny_dlna {

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


/**
 * @class ActionReply
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

  /**
   * @brief Represents a request to invoke a DLNA service action.
   *
   * An ActionRequest bundles the target service (`p_service`), the action
   * name (`action`) and a collection of input arguments. It is used by the
   * control point when building and posting SOAP requests to a service's
   * control URL.
   *
   * Fields:
   * - `p_service`: pointer to the target `DLNAServiceInfo` describing the
   *   service to call (may be null until assigned).
   * - `action`: name of the action to invoke (e.g. "SetAVTransportURI").
   * - `arguments`: list of input `Argument` items to include in the SOAP
   *   request body.
   * - `result_count`: optional counter used by callers to track returned
   *   results.
   *
   * Usage:
   * - Construct with the target service and action name using the
   *   ActionRequest(DLNAServiceInfo&, const char*) constructor.
   * - Add input arguments via `addArgument()`.
   * - The request is then passed to the control point which will serialize
   *   it into SOAP XML and post it to the service.
   */

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