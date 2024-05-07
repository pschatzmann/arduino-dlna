#pragma once
#include "Print.h"

namespace tiny_dlna {

/**
 * @brief Functions to efficiently output XML
 */

struct XMLPrinter {
  XMLPrinter() = default;
  XMLPrinter(Print& output) { setOutput(output); }

  /// Defines the output
  void setOutput(Print& output) { p_out = &output; }

  void printXML() { p_out->println("<?xml version=\"1.0\"?>"); }

  void printNode(const char* node, const char* txt,
                 const char* attributes = nullptr) {
    p_out->print("<");
    p_out->print(node);
    if (attributes) {
      p_out->print(" ");
      p_out->print(attributes);
    }
    p_out->print(">");
    p_out->print(txt);
    p_out->print("</");
    p_out->print(node);
    p_out->println(">");
  }

  void printNode(const char* node, int txt,
                 const char* attributes = nullptr) {
    p_out->print("<");
    p_out->print(node);
    if (attributes) {
      p_out->print(" ");
      p_out->print(attributes);
    }
    p_out->print(">");
    p_out->print(txt);
    p_out->print("</");
    p_out->print(node);
    p_out->println(">");
  }

  void printNode(const char* node, std::function<void(void)> callback,
                 const char* attributes = nullptr) {
    p_out->print("<");
    p_out->print(node);
    if (attributes) {
      p_out->print(" ");
      p_out->print(attributes);
    }
    p_out->print(">");
    callback();
    p_out->print("</");
    p_out->print(node);
    p_out->println(">");
  }

  void printNodeArg(const char* node, std::function<void(void)> callback) {
    p_out->print("<");
    p_out->print(node);
    p_out->print(">");
    callback();
    p_out->print("</");
    p_out->print(node);
    p_out->println(">");
  }

 protected:
  Print* p_out = &Serial;

  void println(const char* txt) { p_out->println(txt); }
};

}  // namespace tiny_dlna