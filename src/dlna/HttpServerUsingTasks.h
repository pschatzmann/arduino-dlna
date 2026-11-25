#pragma once
#include "basic/Logger.h"
#include "concurrency/Task.h"
#include "http/Server/HttpServer.h"

namespace tiny_dlna {

/**
 * @brief Experimental HTTP server implementation that handles each client in a
 * separate task.
 *
 * This class extends HttpServer and, upon a new client connection, spawns a
 * dedicated Task to handle the client. The task runs until the client
 * disconnects, then cleans up.
 *
 * @tparam ClientType The type representing the network client (e.g.,
 * WiFiClient).
 * @tparam ServerType The type representing the network server (e.g.,
 * WiFiServer).
 *
 * Example usage:
 * @code
 * tiny_dlna::HttpServerUsingTasks<WiFiClient, WiFiServer> server(80);
 * @endcode
 */
template <typename ClientType, typename ServerType>
class HttpServerUsingTasks : public HttpServer<ClientType, ServerType> {
 private:
  bool is_started_ = false;

 public:
  HttpServerUsingTasks(ServerType& server, int bufferSize = 1024,
                       int taskStackSize = 1024 * 8, int taskPriority = 1)
      : HttpServer<ClientType, ServerType>(server, bufferSize),
        taskStackSize_(taskStackSize),
        taskPriority_(taskPriority) {}

  ~HttpServerUsingTasks() { end(); }

  bool begin() override {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpServerUsingTasks::begin()");
    if (is_started_) {
      DlnaLogger.log(
          DlnaLogLevel::Warning,
          "HttpServerUsingTasks::begin() called but already started");
      return false;
    }

    bool result = HttpServer<ClientType, ServerType>::begin();
    if (!result) {
      DlnaLogger.log(
          DlnaLogLevel::Error,
          "HttpServerUsingTasks::begin() failed to start base HttpServer");
      return false;
    }
    // Start the main accept loop in a background task
    accept_task_.begin([this]() {
      while (this->isActive()) {
        if (this->server_ptr) {
          ClientType client = this->server_ptr->accept();
          if (client.connected()) {
            client.setNoDelay(true);  // disables Nagle
            tasks_.emplace_back(
                new Task("HttpClientTask", taskStackSize_, taskPriority_));
            Task* clientTask = tasks_.back().get();
            clientTask->begin([this, client, clientTask]() mutable {
              this->handleClientTask(client);
              delay(5);
              DlnaLogger.log(DlnaLogLevel::Debug, "Removing client");
            });
          }
        } else {
          DlnaLogger.log(DlnaLogLevel::Error,
                         "HttpServerUsingTasks::begin() server_ptr is null");
        }
        delay(10);  // avoid busy loop (Arduino delay)
      }
    });
    is_started_ = true;
    DlnaLogger.log(DlnaLogLevel::Info, "HttpServerUsingTasks started: %s",
                   result ? "true" : "false");
    return result;
  }

  void end() override {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpServerUsingTasks::end()");
    if (!is_started_) return;
    accept_task_.end();
    for (auto& t : tasks_) {
      t->end();
    }
    tasks_.clear();
    HttpServer<ClientType, ServerType>::end();
    is_started_ = false;
  }

  bool doLoop() override { return true; }

 protected:
  void handleClientTask(ClientType& client) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "HttpServerUsingTasks: handling new client");
    // Create a dedicated handler for this client
    while (client.connected()) {
      tiny_dlna::HttpClientHandler<ClientType> handler(&client);
      handler.readHttpHeader();
      onRequest(handler.requestHeader().urlPath(), handler);
    }

    DlnaLogger.log(DlnaLogLevel::Debug,
                   "HttpServerUsingTasks: client disconnected");
  }

  void removeTask(Task* task) {
    auto it = std::remove_if(
        tasks_.begin(), tasks_.end(),
        [task](const std::unique_ptr<Task>& t) { return t.get() == task; });
    tasks_.erase(it, tasks_.end());
  }

  Task accept_task_;
  std::vector<std::unique_ptr<Task>> tasks_;
  int taskStackSize_;
  int taskPriority_;

  /// processing logic
  bool onRequest(const char* path, HttpClientHandler<ClientType>& handler) {
    DlnaLogger.log(DlnaLogLevel::Info, "Serving at %s", path);

    bool result = false;
    // check in registered handlers
    StrView pathStr = StrView(path);
    pathStr.replace("//", "/");  // TODO investiage why we get //
    HttpRequestHandlerLine* handler_line_ptr =
        this->findHandler(pathStr, handler.requestHeader().method(),
                          handler.requestHeader().accept());
    if (handler_line_ptr != nullptr) {
      // call registed handler function
      handler_line_ptr->fn(handler, this, path, handler_line_ptr);
      result = true;
    }

    if (!result) {
      DlnaLogger.log(DlnaLogLevel::Error, "Request %s not available", path);
    }

    return result;
  }
};

}  // namespace tiny_dlna
