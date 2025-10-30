#pragma once

#include "basic/Logger.h"
#include "basic/Str.h"

namespace tiny_dlna {

/**
 * @brief URL parser which breaks a full url string up into its individual parts
 *
 * http://pschatzmann.ch:80/path1/path2
 * -> protocol: http
 * -> host: pschatzmann.ch
 * -> port: 80
 * -> url: http://pschatzmann.ch:80/path1/path2
 * -> root: http://pschatzmann.ch:80
 */
class Url {
 public:
  // empty url
  Url() = default;

  ~Url() {
    clear();
  }

  // setup url with string
  Url(const char* url) {
    DlnaLogger.log(DlnaLogLevel::Debug, "Url %s", url);
    setUrl(url);
  }

  // copy constructor
  Url(Url& url) {
    DlnaLogger.log(DlnaLogLevel::Debug, "Url %s", url.url());
    setUrl(url.url());
  }

  const char* url() { return urlStr.c_str(); }
  const char* path() { return pathStr.c_str(); }
  const char* host() { return hostStr.c_str(); }
  const char* protocol() { return protocolStr.c_str(); }
  const char* urlRoot() {
    return urlRootStr.c_str();
  }  // prefix w/o path -> https://host:port
  int port() { return portInt; }

  void setUrl(const char* url) {
    DlnaLogger.log(DlnaLogLevel::Debug, "setUrl %s", url);
    this->urlStr = url;
    parse();
  }

  operator bool() { return !urlStr.isEmpty(); }

  bool operator ==(Url& other) {
    return this->urlStr.equals(other.urlStr.c_str());
  }
  bool operator !=(Url& other) {
    return !(*this == other);
  }

  void clear() {
    pathStr.clear();
    hostStr.clear();
    protocolStr.clear();
    urlRootStr.clear();
    urlStr.clear();
    portInt = -1;
  }

 protected:
  Str pathStr = Str(40);
  Str hostStr = Str(20);
  Str protocolStr = Str(6);
  Str urlRootStr = Str(40);
  Str urlStr = Str(40);
  int portInt;

  void parse() {
    DlnaLogger.log(DlnaLogLevel::Debug, "Url::parse()");

    int protocolEnd = urlStr.indexOf("://");
    if (protocolEnd == -1) {
      return;
    }
    protocolStr.substrView(urlStr, 0, protocolEnd);
    int portStart = urlStr.indexOf(":", protocolEnd + 3);
    int hostEnd =
        portStart != -1 ? portStart : urlStr.indexOf("/", protocolEnd + 4);
    // we have not path -> so then host end is the end of string
    if (hostEnd == -1) {
      hostEnd = urlStr.length();
    }
    hostStr.substrView(urlStr, protocolEnd + 3, hostEnd);
    if (portStart > 0) {
      portInt = atoi(urlStr.c_str() + portStart + 1);
    } else {
      if (protocolStr.startsWith("https"))
        portInt = 443;
      else if (protocolStr.startsWith("http"))
        portInt = 80;
      else if (protocolStr.startsWith("ftp"))
        portInt = 21;
      else
        portInt = -1;  // undefined
    }
    int pathStart = urlStr.indexOf("/", protocolEnd + 4);
    if (pathStart == 0) {
      // we have no path
      pathStr = "/";
      urlRootStr = urlStr;
    } else {
      pathStr.substrView(urlStr, pathStart, urlStr.length());
      pathStr.trim();
      urlRootStr.substrView(urlStr, 0, pathStart);
    }
    DlnaLogger.log(DlnaLogLevel::Debug, "url-> %s", url());
    DlnaLogger.log(DlnaLogLevel::Debug, "path-> %s", path());
  }
};

}  // namespace tiny_dlna
