#pragma once

#include "Print.h"
#include "assert.h"
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include "basic/StrView.h"
#include "basic/Vector.h"
#include "dlna_config.h"

namespace tiny_dlna {
/**
 * @brief Represents a single XML element
 * @author Phil Schatzmann
 */
struct XMLNode {
  /**
   * @brief Name of the XML node
   */
  const char* node = nullptr;
  /**
   * @brief Attributes of the XML node
   */
  const char* attributes = nullptr;
  /**
   * @brief Content of the XML node
   */
  const char* content = nullptr;

  /**
   * @brief Default constructor
   */
  XMLNode() = default;

  /**
   * @brief Constructor with node name, content, and optional attributes
   * @param node Name of the node
   * @param content Content of the node
   * @param attr Optional attributes
   */
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

  /**
   * @brief Default constructor
   */
  XMLPrinter() = default;

  /**
   * @brief Constructor with output
   * @param output Reference to Print object for output
   */
  XMLPrinter(Print& output) { setOutput(output); }

  /**
   * @brief Defines the output Print object
   * @param output Reference to Print object
   */
  void setOutput(Print& output) { p_out = &output; }

  /**
   * @brief Prints the XML header
   * @return Number of bytes written
   */
  size_t printXMLHeader() {
    assert(p_out != nullptr);
    return p_out->println("<?xml version=\"1.0\"?>");
  }

  /**
   * @brief Prints an XML node from XMLNode struct
   * @param node XMLNode struct
   * @return Number of bytes written
   */
  size_t printNode(XMLNode node) {
    return printNode(node.node, node.content, node.attributes);
  }

  /**
   * @brief Prints an XML node with a single child
   * @param node Name of the node
   * @param child Child XMLNode
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
  size_t printNode(const char* node, XMLNode child,
                   const char* attributes = nullptr) {
    Vector<XMLNode> children;
    children.push_back(child);
    return printNode(node, children, attributes);
  }

  /**
   * @brief Prints an XML node with multiple children
   * @param node Name of the node
   * @param children Vector of child XMLNodes
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
  size_t printNode(const char* node, Vector<XMLNode> children,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += printChildren(children);
    result += printNodeEnd(node);
    return result;
  }

  /**
   * @brief Prints an XML node with text content
   * @param node Name of the node
   * @param txt Text content
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
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

  /**
   * @brief Prints an XML node with integer content
   * @param node Name of the node
   * @param txt Integer value
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
  size_t printNode(const char* node, int txt,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBegin(node, attributes);
    result += p_out->print(txt);
    result += printNodeEnd(node);
    return result;
  }

  /**
   * @brief Prints an XML node using a callback for content
   * @param node Name of the node
   * @param callback Function to generate content
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
  size_t printNode(const char* node, std::function<size_t(void)> callback,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += callback();
    result += printNodeEnd(node);
    return result;
  }

  /**
   * @brief Prints an XML node using a callback that receives a context pointer.
   * @param node Name of the node
   * @param callback Function to generate content, takes a void* context
   * @param ref Context pointer passed through to the callback
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
  size_t printNode(const char* node, std::function<size_t(void*)> callback,
                   void* ref, const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += callback(ref);
    result += printNodeEnd(node);
    return result;
  }

  /**
   * @brief Prints an XML node using a plain function pointer that receives a context pointer.
   * @param node Name of the node
   * @param callback Function pointer to generate content, takes a void* context
   * @param ref Context pointer passed through to the callback
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
  size_t printNode(const char* node, size_t (*callback)(void*), void* ref,
                   const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += callback(ref);
    result += printNodeEnd(node);
    return result;
  }

  /**
   * @brief Prints an XML node using a callback that receives the Print& and a context pointer.
   * @param node Name of the node
   * @param callback Function to generate content, takes Print& and void* context
   * @param ref Context pointer passed through to the callback
   * @param attributes Optional attributes
   * @return Number of bytes written
   */
  size_t printNode(const char* node,
                   std::function<size_t(Print&, void*)> callback,
                   void* ref, const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += callback(*p_out, ref);
    result += printNodeEnd(node);
    return result;
  }

  /**
   * @brief Prints an XML node using a plain function pointer that receives Print& and a context pointer.
   */
  size_t printNode(const char* node, size_t (*callback)(Print&, void*),
                   void* ref, const char* attributes = nullptr) {
    assert(p_out != nullptr);
    size_t result = printNodeBeginNl(node, attributes);
    result += callback(*p_out, ref);
    result += printNodeEnd(node);
    return result;
  }

  /**
   * @brief printf-style helper that formats into an internal buffer and writes to the configured Print output.
   * @param fmt Format string
   * @param ... Variable arguments
   * @return Number of bytes written
   */
  size_t printf(const char* fmt, ...) {
    assert(p_out != nullptr);
    char buf[MAX_PRINTF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = ::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return 0;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    return p_out->print(buf);
  }

  /**
   * @brief Helper to print a UPnP <argument> element with name, direction and optional relatedStateVariable.
   * @param name Argument name
   * @param direction Argument direction
   * @param relatedStateVariable Optional related state variable
   * @return Number of bytes written
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
   * @brief Helper to print a UPnP <stateVariable> element with name, dataType and optional sendEvents attribute and inner content callback.
   * @param name State variable name
   * @param dataType State variable data type
   * @param sendEvents Whether to send events
   * @param extra Optional callback for extra content
   * @return Number of bytes written
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

  /**
   * @brief Prints the beginning of an XML node
   * @param node Name of the node
   * @param attributes Optional attributes
   * @param ns Optional namespace
   * @return Number of bytes written
   */
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

  /**
   * @brief Prints the beginning of an XML node and a newline
   * @param node Name of the node
   * @param attributes Optional attributes
   * @param ns Optional namespace
   * @return Number of bytes written
   */
  size_t printNodeBeginNl(const char* node, const char* attributes = nullptr,
                          const char* ns = nullptr) {
    size_t result = printNodeBegin(node, attributes, ns);
    result += p_out->println();
    return result;
  }

  /**
   * @brief Prints the end of an XML node
   * @param node Name of the node
   * @param ns Optional namespace
   * @return Number of bytes written
   */
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

  void clear() {
    p_out = &Serial;
  }

 protected:
  Print* p_out = &Serial;

  size_t println(const char* txt) {
    assert(p_out != nullptr);
    return p_out->println(txt);
  }

  size_t printChildren(Vector<XMLNode>& children) {
    size_t result = 0;
    for (auto& node : children) {
      result += printNode(node);
    }
    return result;
  }
};

}  // namespace tiny_dlna