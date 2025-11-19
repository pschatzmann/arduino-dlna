#pragma once
#include "concurrency/Task.h"
#include "http/Server/HttpServer.h"
#include "basic/Logger.h"

namespace tiny_dlna {

/**
 * @brief HTTP server implementation that runs in a separate task/thread.
 *
 * This class extends HttpServer and manages the server loop in a dedicated task
 * using the Task class. It is templated on the client and server types,
 * allowing flexibility for different network stacks.
 *
 * @tparam ClientType The type representing the network client (e.g.,
 * WiFiClient).
 * @tparam ServerType The type representing the network server (e.g.,
 * WiFiServer).
 *
 * @note The server loop is executed in a background task, which periodically
 * calls doLoop().
 *
 * Example usage:
 * @code
 * tiny_dlna::HttpServerUsingTask<WiFiClient, WiFiServer> server(80);
 * @endcode
 */
template <typename ClientType, typename ServerType>
class HttpServerUsingTask : public HttpServer<ClientType, ServerType> {
 public:
  HttpServerUsingTask(ServerType& server, int bufferSize = 1024,
                      int taskStackSize = 4096, int taskPriority = 1)
      : HttpServer<ClientType, ServerType>(server, bufferSize),
        server_task_("HttpServerTask", taskStackSize, taskPriority) {}

  ~HttpServerUsingTask() { end(); }

  bool begin() override {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpServerUsingTask::begin()");
    bool result = HttpServer<ClientType, ServerType>::begin();
    // start processing loop in background task
    server_task_.begin([this]() {
      while (this->isActive()) {
        HttpServer<ClientType, ServerType>::doLoop();
        delay(1);
      }
    });
    DlnaLogger.log(DlnaLogLevel::Info, "HttpServerUsingTask started: %s",
               result ? "true" : "false");
    return result;
  }

  void end() override {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpServerUsingTask::end()");
    server_task_.end();
    HttpServer<ClientType, ServerType>::end();
  }

  bool doLoop() override { return true; }

 protected:
  Task server_task_;
};

}  // namespace tiny_dlna