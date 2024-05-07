#pragma once

#include "WiFi.h"

namespace tiny_dlna {

/**
 * @brief Abstract class for handling streamed output
 * 
 */

class HttpStreamedOutput {
    public:
        // provides the mime type
        virtual const char* mime() = 0;

        // checks if the client is valid
        virtual bool isValid(WiFiClient &client) = 0;

        // content that is written when the request is opened
        virtual void open(WiFiClient &client) = 0;

        // checks if we have any open clients
        virtual bool isOpen() = 0;

        // end processing by wr
        virtual void close() = 0;

        // write the content to the HttpStreamedMultiOutput
        virtual size_t write( uint8_t *content, int len) = 0;

        // writes a line 
        virtual size_t print(const char* str) = 0;

        // writes a line which terminates with a html line break
        virtual size_t println(const char* str) = 0;

        // release memory
        virtual void cleanup() = 0;

        // actually we do nothing here - but some subclasses might
        virtual void doLoop() = 0;
};

}

