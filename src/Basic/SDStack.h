#pragma once


#include "SPI.h"
#include "SD.h"
#include "Basic/StrExt.h"

namespace tiny_dlna {

/**
 * @brief Using a SD file for stack operations on a entries which 
 * are potentially of different size.
 * 
 */
class SDStack {
    public:
        SDStack(const char* fileName, bool resume=false){
            HttpLogger.log(Info,"SDStack %s", fileName);
            this->file_name = fileName;
            if (resume) {
                File file = SD.open(fileName);
                this->max_position = file.size();   
                file.close();
            } else {
                this->max_position = 0;   
            }
        }

        // delete the content
        void reset() {
            SD.remove(this->file_name);
            this->max_position = 0;   
            this->result_string.clear();
        }

        // push a string
        void* push(const char* data){
            HttpLogger.log(Info,"SDStack %s %s", "push", data);
            int len = strlen(data);
            // write with trailing 0
            push(data, len+1);
        }

        // adds some data and the record size to the end
        void* push(const void* data, int len){
            HttpLogger.log(Info,"SDStack %s", "push");
            File file = SD.open(file_name, FILE_WRITE);
            if (file){
                file.seek(max_position);
                file.write((uint8_t*)data, len);

                file.write((uint8_t*)&len, position_size);
                max_position += (len + position_size);
                file.close();
            } else {
                HttpLogger.log(Error,"SDStack::push - Could not open file: %s", file_name);
            }
        } 

        // pops data from the end. We return a pointer to an internal buffer which contains the data 
        // - if the stack is empty we return nullptr
        void* pop(int &len) {
            void* result = nullptr;

            File file = SD.open(file_name);
            if (file && max_position > 0){
                // read last size
                if (!file.seek(max_position - position_size)){
                    HttpLogger.log(Error,"SDStack::pop", "seek failed");
                }
                // read record length
                int len;
                file.read((uint8_t*)&len, position_size);

                // read record into buffer of stirng
                result_string.setCapacity(len);
                int new_position = max_position - position_size - len;
                file.seek(new_position);
                file.read((uint8_t*)result_string.c_str(), len);
                // update end
                max_position = new_position;
                result = (void*)result_string.c_str();
                len = result_string.length();
                file.close();

                if (max_position<=position_size) {
                    HttpLogger.log(Info,"SDStack::pop %s", "last entry");
                    max_position = 0;
                    len = 0;
                }

            } else {
                HttpLogger.log(Info,"SDStack::pop %s", "no data");
                len = 0;
            }
            return result;

        }

        // convinience function to pop for a c strings
        const Str popStr() {
            HttpLogger.log(Info,"SDStack %s", "popStr");
            int len;
            char* str = (char*)pop(len);
            return Str(str, len, len);
        }

    protected:
        const char* file_name;
        const int position_size = sizeof(int);
        int max_position;
        StrExt result_string = StrExt(80);

};

}

