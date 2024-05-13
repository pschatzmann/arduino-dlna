namespace tiny_dlna {

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
