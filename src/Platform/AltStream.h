#pragma once

#ifndef PLATFORM_LINUX
#include <Stream.h>
#else

namespace tiny_dlna {

/**
 * @brief We provide the Stream class so that it complies outside of the Arduino Environment. 
 * 
 */
class Stream {
  public:
    virtual void print(const char* str="") = 0;
    virtual void println(const char* str="") = 0;
    virtual void flush() = 0;
    virtual void write(const char* str, int len) = 0;
    virtual void write(uint8_t* str, int len) = 0;

};


void delay(int n){

};

/**
 * @brief We use the SerialDef class to be able to provide Serail, Serial1 and Serial2 outside of the
 * Arduino environment;  
 * 
 */
class SerialDef : public Stream {
  public:
    virtual void begin(int speed){
        // nothing to be done
    }
    virtual void print(const char* str){
        std::cout << str;
        std::cout.flush();
    }
    virtual void println(const char* str=""){
        std::cout << str << "\n";
        std::cout.flush();
    }
    virtual void print(int str){
        std::cout << str;
        std::cout.flush();
    }
    virtual void println(int str){
        std::cout << str << "\n";
        std::cout.flush();
    }
    virtual void flush() {
        std::cout.flush();
    }
    virtual void write(const char* str, int len) {
        std::cout.write(str, len);
    }
    virtual void write(uint8_t* str, int len) {
        std::cout.write((const char*)str, len);
    }

};

SerialDef Serial;
SerialDef Serial1;
SerialDef Serial2;

const int WL_CONNECTED = 1;

/**
 * @brief We provide the WiFi class, so that the demo applcations can be compiled outside of the 
 * Arduino Environment.
 * 
 */
class WifiMock {
  public:
    virtual void begin(const char*name, const char* pwd){
        // nothing to be done
    }
    // we assume the network to be available on a desktop or host machine
    int status() {
        return WL_CONNECTED;
    }

};

WifiMock WiFi;


}

#endif // PLATFORM_LINUX
