#pragma once

#include "HttpHeader.h"

namespace tiny_dlna {


// forward declarations for the callback
class HttpServer;
class HttpRequestHandlerLine;

// Callback function which provides result
typedef void (*web_callback_fn)(HttpServer *server, const char* requestPath, HttpRequestHandlerLine *handlerLine);

/**
 * @brief Used to register and process callbacks 
 * 
 */
class HttpRequestHandlerLine {
    public:
        HttpRequestHandlerLine(int ctxSize=0){
            Logger.log(Info,"HttpRequestHandlerLine");
            contextCount = ctxSize;
            context = new void*[ctxSize];
        }

        ~HttpRequestHandlerLine(){
            Logger.log(Info,"~HttpRequestHandlerLine");
            if (contextCount>0){
                Logger.log(Info,"HttpRequestHandlerLine %s","free");
                delete[] context;
            }
        }

        TinyMethodID method;
        const char* path = nullptr;
        const char* mime = nullptr;
        web_callback_fn fn;
        void** context;
        int contextCount;
        StrView *header = nullptr;
};

}

