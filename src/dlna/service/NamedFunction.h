namespace tiny_dlna {

/**
 * @brief DLNA Service: Named callback function
 * @author Phil Schatzmann
*/

  struct NamedFunction {
    const char* name;
    std::function<void(void)> func;
    NamedFunction(const char* name, std::function<void(void)> func) {
      this->name = name;
      this->func = func;
    }
    NamedFunction() = default;
  };

}
