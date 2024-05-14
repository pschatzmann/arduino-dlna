#pragma once

#include "Print.h"
#include "assert.h"
#include "basic/StrView.h"
#include "basic/Vector.h"

namespace tiny_dlna {
/**
 * @brief Represents a single XML element
 * @author Phil Schatzmann
*/
struct XMLNode {
  const char* node;
  const char* attributes;
  const char* content;
  XMLNode(const char* node, const char* content, const char* attr = nullptr) {
    this->node = node;
    this->attributes = attr;
  }
};

/**
 * @brief Functions to efficiently output XML. XML data contains a lot of
 * redundancy so it is more memory efficient to generate the data instead of
 * using a predefined XML document.
 * @author Phil Schatzmann
 */

struct XMLPrinter {
  XMLPrinter() = default;
  XMLPrinter(Print& output) { setOutput(output); }

  /// Defines the output
  void setOutput(Print& output) { p_out = &output; }

  size_t printXMLHeader() {
    assert(p_out != nullptr);
    return p_out->println("<?xml version=\"1.0\"?>");
  }

  size_t printNode(XMLNode node) {
    return printNode(node.node, node.content, node.attributes);
  }

  size_t printNode(const char* node, Vector<XMLNode> children,
                 const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = 0;
    result += p_out->print("<");
    result += p_out->print(node);
    if (attributes != nullptr) {
      result += p_out->print(" ");
      result += p_out->print(attributes);
    }
    result += p_out->println(">");
    result += printChildren(children);
    result += p_out->print("</");
    result += p_out->print(node);
    result += p_out->println(">");
    return result;
  }

  size_t printNode(const char* node, const char* txt,
                 const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = 0;

    result += p_out->print("<");
    result += p_out->print(node);
    if (attributes) {
      result += p_out->print(attributes);
    }
    if (StrView(txt).isEmpty()) {
      result += p_out->println("/>");
    } else {
      result += p_out->print(">");
      result += p_out->print(txt);
      result += p_out->print("</");
      result += p_out->print(node);
      result += p_out->println(">");
    }
    return result;
  }

  size_t printNode(const char* node, int txt, const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = 0;

    result += p_out->print("<");
    result += p_out->print(node);
    if (attributes) {
      result += p_out->print(attributes);
    }
    result += p_out->print(">");
    result += p_out->print(txt);
    result += p_out->print("</");
    result += p_out->print(node);
    result += p_out->println(">");
    return result;
  }

  size_t printNode(const char* node, std::function<size_t(void)> callback,
                 const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = 0;

    result += p_out->print("<");
    result += p_out->print(node);
    if (attributes != nullptr) {
      result += p_out->print(" ");
      result += p_out->print(attributes);
    }
    result += p_out->println(">");
    result += callback();
    result += p_out->print("</");
    result += p_out->print(node);
    result += p_out->println(">");
    return result;
  }

  size_t printNodeArg(const char* node, std::function<size_t(void)> callback) {
    assert(p_out != nullptr);
    size_t result = 0;

    result += p_out->print("<");
    result += p_out->print(node);
    result += p_out->println(">");
    result += callback();
    result += p_out->print("</");
    result += p_out->print(node);
    result += p_out->println(">");
    return result;
  }

 protected:
  Print* p_out = &Serial;

  size_t println(const char* txt) {
    assert(p_out != nullptr);
    return p_out->println(txt);
  }

 protected:
  size_t printChildren(Vector<XMLNode> &children) {
    size_t result = 0;
    for (auto& node : children) {
      result += printNode(node);
    }
    return result;
  }
};

}  // namespace tiny_dlna