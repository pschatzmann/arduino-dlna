#pragma once

#include "Print.h"
#include "assert.h"
#include <cstring>
#include "basic/StrView.h"
#include "basic/Vector.h"

namespace tiny_dlna {
/**
 * @brief Represents a single XML element
 * @author Phil Schatzmann
 */
struct XMLNode {
  const char* node = nullptr;
  const char* attributes = nullptr;
  const char* content = nullptr;
  XMLNode() = default;
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

  size_t printNode(const char* node, XMLNode child,
                   const char* attributes = nullptr) {
    Vector<XMLNode> children;
    children.push_back(child);
    return printNode(node, children, attributes);
  }

  size_t printNode(const char* node, Vector<XMLNode> children,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += printChildren(children);
    result += printNodeEnd(node);
    return result;
  }

  size_t printNode(const char* node, const char* txt = nullptr,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    int result = 0;
    if (StrView(txt).isEmpty()) {
      result += p_out->print("<");
      result += p_out->print(node);
      result += p_out->println("/>");
    } else {
      result += printNodeBegin(node, attributes);
      result += p_out->print(txt);
      result += printNodeEnd(node);
    }
    return result;
  }

  size_t printNode(const char* node, int txt,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBegin(node, attributes);
    result += p_out->print(txt);
    result += printNodeEnd(node);
    return result;
  }

  size_t printNode(const char* node, std::function<size_t(void)> callback,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += callback();
    result += printNodeEnd(node);
    return result;
  }

  /**
   * Helper to print a UPnP <argument> element with name, direction and optional
   * relatedStateVariable.
   */
  size_t printArgument(const char* name, const char* direction,
                       const char* relatedStateVariable = nullptr) {
    assert(p_out != nullptr);
    size_t result = 0;
    result += printNodeBeginNl("argument");
    result += printNode("name", name);
    result += printNode("direction", direction);
    if (relatedStateVariable && relatedStateVariable[0]) {
      result += printNode("relatedStateVariable", relatedStateVariable);
    }
    result += printNodeEnd("argument");
    return result;
  }

  /**
   * Helper to print a UPnP <stateVariable> element with name, dataType and
   * optional sendEvents attribute and inner content callback.
   */
  size_t printStateVariable(const char* name, const char* dataType,
                            bool sendEvents = false,
                            std::function<void()> extra = nullptr) {
    assert(p_out != nullptr);
    char attrBuf[32];
    if (sendEvents)
      strcpy(attrBuf, "sendEvents=\"yes\"");
    else
      strcpy(attrBuf, "sendEvents=\"no\"");
    size_t result = 0;
    result += printNodeBeginNl("stateVariable", attrBuf);
    result += printNode("name", name);
    result += printNode("dataType", dataType);
    if (extra) extra();
    result += printNodeEnd("stateVariable");
    return result;
  }

  size_t printNodeBegin(const char* node, const char* attributes = nullptr,
                        const char* ns = nullptr) {
    assert(p_out != nullptr);
    size_t result = 0;
    result += p_out->print("<");
    if (ns != nullptr) {
      result += p_out->print(ns);
      result += p_out->print(":");
    }
    result += p_out->print(node);
    if (attributes != nullptr) {
      result += p_out->print(" ");
      result += p_out->print(attributes);
    }
    result += p_out->print(">");
    return result;
  }

  size_t printNodeBeginNl(const char* node, const char* attributes = nullptr,
                          const char* ns = nullptr) {
    size_t result = printNodeBegin(node, attributes, ns);
    result = +p_out->println();
    return result;
  }

  size_t printNodeEnd(const char* node, const char* ns = nullptr) {
    size_t result = p_out->print("</");
    if (ns != nullptr) {
      result += p_out->print(ns);
      result += p_out->print(":");
    }
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
  size_t printChildren(Vector<XMLNode>& children) {
    size_t result = 0;
    for (auto& node : children) {
      result += printNode(node);
    }
    return result;
  }
};

}  // namespace tiny_dlna