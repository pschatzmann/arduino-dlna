#pragma once

#include "HttpChunkWriter.h"

namespace tiny_dlna {

/**
 * @brief Processing of a single stream to a single client. 
 * In the loop we can simply provide individual small chunks
 * until we are done.
 * 
 * We work on a copy of the WiFiClient
 */

class HttpStreamCopy {
    public:
        HttpStreamCopy(Stream &input, WiFiClient &client, int outputSize=215){
            this->is_open=true;
            this->input_ptr = &input;
            this->client_ptr = &client;
            this->output_size = outputSize;
        }

        ~HttpStreamCopy(){
            close();
        }

        bool isOpen(){
            return is_open;
        }

        void doLoop(){
            if (is_open) {
                char buffer[output_size];
                if(input_ptr->available()>0 && client_ptr->connected()){
                    int len = input_ptr->readBytes(buffer, output_size);
                    writer.writeChunk(*client_ptr, buffer, len);
                } else {
                    close();
                }
            }
        }

    protected:
        HttpChunkWriter writer;
        Stream *input_ptr;
        Client* client_ptr;
        int output_size;
        bool is_open;


        void close() {
            if (is_open){
                is_open = false;
                //input_ptr->close();
                writer.writeEnd(*client_ptr);
                client_ptr = nullptr;    
            }            
        }

};

}
