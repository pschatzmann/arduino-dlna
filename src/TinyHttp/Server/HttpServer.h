#pragma once

#include <stdlib.h> 
#include "Basic/List.h"
#include "Server.h"
#include "Client.h"
#include "HttpHeader.h"
#include "HttpRequestHandlerLine.h"
#include "HttpRequestRewrite.h"
#include "HttpChunkWriter.h"
#include "HttpTunnel.h"
#include <WiFi.h>
#include "HardwareSerial.h"


namespace tiny_dlna {

/**
 * @brief A Simple Header only implementation of Http Server that allows the registration of callback
 * functions. This is based on the Arduino Server class.
 * 
 */
class HttpServer {
    public:
        HttpServer(WiFiServer &server_ptr, int bufferSize=1024){
            DlnaLogger.log(DlnaInfo,"HttpServer");
            this->server_ptr = &server_ptr;
            this->buffer_size = bufferSize;
            this->buffer = new char[bufferSize];
        }

        ~HttpServer(){
            DlnaLogger.log(DlnaInfo,"~HttpServer");
            delete []buffer;
            handler_collection.clear();
            request_header.clear(false);
            reply_header.clear(false);
            rewrite_collection.clear();
        }

        /// Starts the server on the indicated port - calls WiFi.begin(ssid, password);
        bool begin(int port, const char* ssid, const char* password){
#
            if (WiFi.status() != WL_CONNECTED && ssid!=nullptr && password!=nullptr){
                WiFi.begin(ssid, password);
                while (WiFi.status() != WL_CONNECTED) {        
                    delay(500);
                    Serial.print(".");
                }

                Serial.println();
                Serial.print("Started Server at ");
                Serial.print(WiFi.localIP());
                Serial.print(":");
                Serial.println(port);

            }
            return begin(port);
        }

        /// Provides the local ip address
        IPAddress &localIP() {
            static IPAddress address;
            address = WiFi.localIP();
            return address;
        }

        /// Starts the server on the indicated port
        bool begin(int port){
            DlnaLogger.log(DlnaInfo,"HttpServer begin at port %d", port);
            is_active = true;
            server_ptr->begin(port);
            return true;
        }

        /// stops the server_ptr
        void end(){
            DlnaLogger.log(DlnaInfo,"HttpServer %s","stop");
            is_active = false;
        }

        /// adds a rewrite rule
        void rewrite(const char* from, const char* to){
            HttpRequestRewrite *line = new HttpRequestRewrite(from, to);
            rewrite_collection.push_back(line);
        }


        /// register a generic handler
        void on(const char* url, TinyMethodID method, web_callback_fn fn,void* ctx[]=nullptr, int ctxCount=0){
            DlnaLogger.log(DlnaInfo,"Serving at %s",url);
            HttpRequestHandlerLine *hl = new HttpRequestHandlerLine(ctxCount);
            hl->path = url;
            hl->fn = fn;
            hl->method = method;
            //hl->context = ctx;
            memmove(hl->context, ctx, ctxCount * sizeof(void*));
            hl->contextCount = ctxCount;
            addHandler(hl);
        }

        /// register a handler with mime
        void on(const char* url, TinyMethodID method, const char* mime, web_callback_fn fn){
            DlnaLogger.log(DlnaInfo,"Serving at %s",url);
            HttpRequestHandlerLine *hl = new HttpRequestHandlerLine();
            hl->path = url;
            hl->fn = fn;
            hl->method = method;
            hl->mime = mime;
            addHandler(hl);
        }


        /// register a handler which provides the indicated string
        void on(const char* url, TinyMethodID method, const char* mime, const char* result) {
            DlnaLogger.log(DlnaInfo,"Serving at %s",url);

            auto lambda = [](HttpServer *server_ptr,const char*requestPath, HttpRequestHandlerLine *hl) { 
                DlnaLogger.log(DlnaInfo,"on-strings %s","lambda");
                if (hl->contextCount<2){
                    DlnaLogger.log(DlnaError,"The context is not available");
                    return;
                }
                const char* mime = static_cast<StrView*>(hl->context[0])->c_str();
                const char* msg = static_cast<StrView*>(hl->context[1])->c_str();
                server_ptr->reply(mime, msg, 200);
            };
            HttpRequestHandlerLine *hl = new HttpRequestHandlerLine(2);
            hl->context[0] = new StrView(mime);
            hl->context[1] = new StrView(result);
            hl->path = url;
            hl->fn = lambda;
            hl->method = method;
            addHandler(hl);
        }
  
        /// register a redirection
        void on(const char*url, TinyMethodID method, Url &redirect){
            DlnaLogger.log(DlnaInfo,"Serving at %s",url);
            auto lambda = [](HttpServer *server_ptr, const char*requestPath, HttpRequestHandlerLine *hl) { 
                if (hl->contextCount<1){
                    DlnaLogger.log(DlnaError,"The context is not available");
                    return;
                }
                DlnaLogger.log(DlnaInfo,"on-redirect %s","lambda");
                HttpReplyHeader reply_header;
                Url *url = static_cast<Url*>(hl->context[0]);
                reply_header.setValues(301, "Moved");
                reply_header.put(LOCATION, url->url());
                reply_header.put("X-Forwarded-Host",(const char*)hl->context[1]);
                reply_header.write(server_ptr->client());
                server_ptr->endClient(); 
            };

            HttpRequestHandlerLine *hl = new HttpRequestHandlerLine(2);
            const char* lh = localHost();
            hl->context[0] = new Url(redirect);
            hl->context[1] = (void*) lh;
            hl->path = url;
            hl->fn = lambda;
            hl->method = method;
            addHandler(hl);
        }

        /// register a redirection
        void on(const char*url, TinyMethodID method, HttpTunnel &tunnel){
            DlnaLogger.log(DlnaInfo,"Serving at %s",url);

            auto lambda = [](HttpServer *server_ptr,const char*requestPath, HttpRequestHandlerLine *hl) { 
                DlnaLogger.log(DlnaInfo,"on-HttpTunnel %s","lambda");
                HttpTunnel *p_tunnel = static_cast<HttpTunnel*>(hl->context[0]);
                if (p_tunnel==nullptr){
                    DlnaLogger.log(DlnaError,"p_tunnel is null");
                    server_ptr->replyNotFound();
                    return;
                }
                const char* mime = hl->mime;
                // execute T_GET request 
                Stream *p_in = p_tunnel->get();
                if (p_in==nullptr){
                    DlnaLogger.log(DlnaError,"p_in is null");
                    server_ptr->replyNotFound();
                    return;
                }
                const char* content_len = p_tunnel->request().reply().get(CONTENT_LENGTH);
                StrView content_len_str{content_len};
                // provide result
                server_ptr->reply(mime, *p_in, content_len_str.toInt());
            };

            HttpRequestHandlerLine *hl = new HttpRequestHandlerLine(1);
            hl->path = url;
            hl->method = method;
            hl->mime = tunnel.mime();
            hl->context[0] = &tunnel;
            hl->fn = lambda;
            addHandler(hl);
        }

        /// generic handler - you can overwrite this method to provide your specifc processing logic
        bool onRequest(const char* path) {
            DlnaLogger.log(DlnaInfo,"Serving at %s",path);

            bool result = false;
            // check in registered handlers
            StrView pathStr = StrView(path);
            for (auto it = handler_collection.begin() ; it != handler_collection.end(); ++it) {
                HttpRequestHandlerLine *handler_line_ptr = *it;
                DlnaLogger.log(DlnaInfo,"onRequest - checking: %s %s %s", nullstr(handler_line_ptr->path.c_str()), methods[handler_line_ptr->method], nullstr(handler_line_ptr->mime));

                if (pathStr.matches(handler_line_ptr->path.c_str()) 
                && request_header.method() == handler_line_ptr->method
                && matchesMime(handler_line_ptr->mime, request_header.accept())) {
                    // call registed handler function
                    DlnaLogger.log(DlnaInfo,"onRequest %s","->found");
                    handler_line_ptr->fn(this, path, handler_line_ptr);
                    result = true;
                    break;
                }
            }
            return result;
        }

        /// chunked reply with data from an input stream
        void replyChunked(const char* contentType, Stream &inputStream, int status=200, const char* msg=SUCCESS) {
            replyChunked(contentType, status, msg);
            HttpChunkWriter chunk_writer;
            while (inputStream.available()){
                int len = inputStream.readBytes(buffer, buffer_size);
                chunk_writer.writeChunk(*client_ptr,(const char*)buffer, len);
            }
            // final chunk
            chunk_writer.writeEnd(*client_ptr);
            endClient();
        }

        /// start of chunked reply: use HttpChunkWriter to provde the data 
        void replyChunked(const char* contentType, int status=200, const char* msg=SUCCESS) {
            DlnaLogger.log(DlnaInfo,"reply %s","replyChunked");
            reply_header.setValues(status, msg);
            reply_header.put(TRANSFER_ENCODING,CHUNKED);
            reply_header.put(CONTENT_TYPE,contentType);
            reply_header.put(CONNECTION,CON_KEEP_ALIVE);
            reply_header.write(this->client());
        }

        /// write reply - copies data from input stream with header size
        void reply(const char* contentType, Stream &inputStream, int size, int status=200, const char* msg=SUCCESS){
            DlnaLogger.log(DlnaInfo,"reply %s","stream");
            reply_header.setValues(status, msg);
            reply_header.put(CONTENT_LENGTH,size);
            reply_header.put(CONTENT_TYPE,contentType);
            reply_header.put(CONNECTION,CON_KEEP_ALIVE);
            reply_header.write(this->client());

            while (inputStream.available()){
                int len = inputStream.readBytes(buffer, buffer_size);
                int written = client_ptr->write((const uint8_t*)buffer, len);
            }
            //inputStream.close();
            endClient();
        }

        /// write reply - using callback that writes to stream
        void reply(const char* contentType, void(*callback)(Stream&out), int status=200, const char* msg=SUCCESS){
            DlnaLogger.log(DlnaInfo,"reply %s","callback");
            reply_header.setValues(status, msg);
            reply_header.put(CONTENT_TYPE,contentType);
            reply_header.put(CONNECTION,CON_KEEP_ALIVE);
            reply_header.write(this->client());
            callback(*client_ptr);
            //inputStream.close();
            endClient();
        }

        /// write reply - using callback that writes to stream
        void reply(const char* contentType, void(*callback)(Print&out), int status=200, const char* msg=SUCCESS){
            DlnaLogger.log(DlnaInfo,"reply %s","callback");
            reply_header.setValues(status, msg);
            reply_header.put(CONTENT_TYPE,contentType);
            reply_header.put(CONNECTION,CON_KEEP_ALIVE);
            reply_header.write(this->client());
            callback(*client_ptr);
            //inputStream.close();
            endClient();
        }


        /// write reply - string with header size
        void reply(const char* contentType, const char* str, int status=200, const char* msg=SUCCESS){
            DlnaLogger.log(DlnaInfo,"reply %s","str");
            int len = strlen(str);
            reply_header.setValues(status, msg);
            reply_header.put(CONTENT_LENGTH,len);
            reply_header.put(CONTENT_TYPE,contentType);
            reply_header.put(CONNECTION,CON_KEEP_ALIVE);
            reply_header.write(this->client());
            client_ptr->write((const uint8_t*)str, len);
            endClient();
        }

        /// write OK reply with 200 SUCCESS
        void replyOK() {
            reply(200, SUCCESS );
        }

        /// write 404 reply 
        void replyNotFound() {
            DlnaLogger.log(DlnaInfo,"reply %s","404");
            reply(404,"Page Not Found" );
        }

        /// Writes the status and message to the reply
        void reply(int status, const char* msg) {
            DlnaLogger.log(DlnaInfo,"reply %d",status);
            reply_header.setValues(status, msg);
            reply_header.write(this->client());
            endClient();
        }

        /// provides the request header
        HttpRequestHeader &requestHeader() {
            return request_header;
        }     

        /// provides the reply header
        HttpReplyHeader & replyHeader() {
            return reply_header;
        }   

        /// closes the connection to the current client_ptr
        void endClient() {
            DlnaLogger.log(DlnaInfo,"HttpServer %s","endClient");
            client_ptr->flush();
            client_ptr->stop();
        }

        /// print a CR LF
        void crlf() {
            client_ptr->print("\r\n");
            client_ptr->flush();
        }

        /// adds a new handler
        void addHandler(HttpRequestHandlerLine *handlerLinePtr){
            handler_collection.push_back(handlerLinePtr);
        }

        /// Legacy method: same as copy();
        void doLoop(){
            copy();
        }

        /// Call this method from your loop!
        void copy(){
            // get the actual client_ptr
            if (is_active) {
                WiFiClient client = server_ptr->available();
                if (client){
                    DlnaLogger.log(DlnaInfo,"doLoop->hasClient");
                    client_ptr = &client;

                    // process the new client with standard functionality
                    if (client.available()>5) {
                        processRequest();
                    }

                    // process doLoop of all registed (and opened) extension_collection 
                    //processExtensions();
                } else {
                    // give other tasks a chance 
                    delay(no_connect_dela);
                }
            }
        }

        /// Provides the current client
        Client &client() {
            return *client_ptr;
        }

        /// Provides true if the server has been started
        operator bool(){
            return is_active;
        }

        /// Determines the local ip address 
        const char *localHost() {
            if (local_host==nullptr){
                local_host = toStr(WiFi.localIP());
            }
            return local_host;
        }

        void setNoConnectDelay(int delay){
            no_connect_dela = delay;
        }

    protected:
        // data
        HttpRequestHeader request_header;
        HttpReplyHeader reply_header;
        List<HttpRequestHandlerLine*> handler_collection;
        //List<Extension*> extension_collection;
        List<HttpRequestRewrite*> rewrite_collection;
        Client *client_ptr;
        WiFiServer *server_ptr;
        bool is_active;
        char* buffer;
        const char* local_host=nullptr;
        int buffer_size;
        int no_connect_dela = 50;

        /// Converts null to an empty string
        const char* nullstr(const char* in){
            return in==nullptr ? "":in;
        }


        // process a full request and send the reply
        void processRequest(){
            DlnaLogger.log(DlnaInfo,"processRequest");
            request_header.read(this->client());
            // provide reply with empty header
            reply_header.clear();
            // determine the path
            const char* path = request_header.urlPath();
            path = resolveRewrite(path);
            bool processed = onRequest(path);
            if (!processed){
              //  replyNotFound();
            }                   
        }

        /// determiens the potentially rewritten url which should be used for the further processing
        const char *resolveRewrite(const char* from){
             for (auto i = rewrite_collection.begin(); i != rewrite_collection.end(); ++i) {
                HttpRequestRewrite *rewrite = (*i);
                if (rewrite->from.matches(from)){
                    return rewrite->to.c_str();
                }
            }    
            return from;
        }

        /// compares mime of handler with mime of request: provides true if they match of one is null (=any value)
        bool matchesMime(const char *handler_mime, const char* request_mime) {
            if (handler_mime==nullptr || request_mime==nullptr){
                return true;
            }
            bool result = StrView(request_mime).contains(handler_mime);
            return result;
        }

};

}

