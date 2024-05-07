#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "HttpRequest.h"
#include "Utils/MimeResolver.h"
#include "Basic/Logger.h"
#include "Utils/SDFileNameMgr.h"
#include "Url.h"
#include "Utils/UrlExtractor.h"
#include "Basic/SDStack.h"

namespace tiny_dlna {

/**
 * @brief Functionality which dumps a website to a SD drive. When the simulate flag is set we do the regular
 * processing but we do not write and information to the SD drive.
 */

class WebCopy {
  public:
    /// default constructor
    WebCopy(Client &client, bool inDoLoop=true, int cpin=-1, int bufferSize=512) {
      Logger.log(Info,"WebCopy");
      this->processing_in_do_loop = inDoLoop;
      this->http.setClient(client);
      this->buffer_size = bufferSize;

      http.reply().put("Content-Type","");
      http.reply().setAutoCreateLines(false);

      // setup SD
      if (cpin==-1){
          SD.begin();
      } else {
          SD.begin(cpin);
      }
    }

    /// Destructor - we release the used memory
    ~WebCopy() {
      clear();
    }

    // defines an alternative file name manager. 
    virtual void setFileNameMgr(SDFileNameMgr &fileNameMgr) {
        this->file_name_mgr = fileNameMgr;
    }

    // starts the extraction for the indicated url
    virtual void start(const char* startUrlChar){
      char msg[100];
      sprintf(msg, "start %s",startUrlChar);
      Logger.log(Info,"WebCopy %s", msg);
      this->start_url.setUrl(startUrlChar);
      const char* root = start_url.urlRoot();
      Logger.log(Info,"WebCopy->root %s", root);


      this->file_name_mgr.setRootUrl(root);
      this->url_extractor.setRootUrl(root);
      // star the dump
      stack.push(start_url.url());
      active = true;
      if (!processing_in_do_loop){
        startDump();
      }
    }

    virtual void stop() {
      Logger.log(Info,"WebCopy %s", "stop");
      active = false;
    }

    bool isActive() {
      return active;
    }

    // incremental processing in Loop
    void doLoop(){
      if (processing_in_do_loop){
        Logger.log(Info, "startDump");
        StrView url = stack.popStr();
        if(active && !url.isEmpty()){
          reportHeap();
          // extract the content
          processContent(url);
        } else {
          active = false;
          clear();
        }
      }
    }

  protected:
    Url start_url;
    Url url;
    HttpRequest http;
    SDFileNameMgr file_name_mgr;
    UrlExtractor url_extractor;
    SDStack stack = SDStack("/stack.txt");
    int max_level;
    int buffer_size;
    bool active;
    bool processing_in_do_loop;


    virtual void clear() {
      stack.reset();
      reportHeap();
    }

    /// dumps the url to a file while for all stack
    virtual void startDump(){
      Logger.log(Info, "startDump");
      StrView url = stack.popStr();
      while(active && !url.isEmpty()){
        reportHeap();
        // extract the content
        processContent(url);
      }
      clear();
    }

    void reportHeap() {
  #ifdef ESP32
      int freeHeap = ESP.getFreeHeap();
      char msg[80];
      sprintf(msg, "free memory: %d",  freeHeap);
      Logger.log(Info, msg);
  #endif
    }

    // copies the content of the url to a file and collects the contained urls
    void processContent(StrView urlStr) {
        Logger.log(Info, "processContent %s", urlStr.c_str());
        if (!urlStr.isEmpty()){
          // Determine Mime
          url.setUrl(urlStr.c_str());
          StrView mimeStr = getMime(url);
          // optional filter by requested mime type
          // create file 
          bool exists = false;
          File file = createFile(urlStr, mimeStr);
          if (file.size()==0){
              // get the data
              reportHeap();
              Logger.log(Info, "processContent %s", url.url());
              http.get(url);
              processFile(file, mimeStr);
          }
          file.close();
        }
    }

    // read from URL to File
    void processFile(File &file, StrView &mimeStr){
        Logger.log(Info, "processFile %s", file.name());
        if (mimeStr.contains("htm")){
          processHtml(file);
        } else {
          processOthers(file);
        }
    }

    // determines the mime type
    StrView getMime(Url &url) {
        Logger.log(Info, "getMime %s", url.url());
        http.head(url);
        const char* mime = http.reply().get(CONTENT_TYPE);
        // text/html; charset=UTF-8 -> html
        StrView mimeStr(mime);
        Logger.log(Info, "getMime-> %s", mimeStr.c_str());
        return mimeStr;
    }

    // creates an empty file for the url - returns true if an empty file was created
    bool createEmptyFile(StrView urlStr){
        Url url(urlStr.c_str());
        StrView mime = getMime(url);
        StrView fileName = this->file_name_mgr.getName(urlStr.c_str(), mime.c_str());
        bool exists = SD.exists(fileName.c_str());
        if (!exists){
          File file = SD.open(fileName.c_str(), FILE_WRITE);
          file.close();
        }
        return !exists;
    }

    // creates the directoy and the file on the SD drive
    File createFile(StrView &urlStr, StrView &mime){
        Logger.log(Info, "createFile %s", urlStr.c_str());
        // determine the file name which is valid for the SD card
        StrView file_name = file_name_mgr.getName(urlStr.c_str(), mime.c_str());
        Logger.log(Info, "createFile %s", file_name.c_str());
        // create directory - limit name to show only the path
        int pos = file_name.lastIndexOf("/");
        if (pos>1){
          file_name.setLength(pos);
          SD.mkdir(file_name.c_str());
          // remove limit to get the full name back
          file_name.setLengthUndo();
        }
        // return an real file only if it does 
        File file = SD.open(file_name.c_str());
        Logger.log(Info, "createFile %s", file.name());

        return file;
    }

    // process a html file by saving the content to the file and extracting the 
    // contained urls
    void processHtml(File &file) {
        Logger.log(Info, "processHtml %s", file.name());
        // process all lines
        uint8_t buffer[buffer_size];
        while(http.available()){
            int len = http.readln(buffer, buffer_size);
            extractReferences(buffer, len);
            // write line to file
            file.write((const uint8_t*)buffer, len);
        }
        file.close();
    }

    // just save the content to a file
    void processOthers(File &file) {
        Logger.log(Info, "processOthers");
        uint8_t buffer[buffer_size];
        while(http.available()){
            // read a single line
            int len = http.readln(buffer, buffer_size);
            file.write((const uint8_t*)buffer, len);
        }
        file.close();
    }

    // extracts the stack and puts them on the stack 
    void extractReferences(uint8_t* buffer, int len) {
        Logger.log(Info, "extractReferences");
        char url_buffer[200];
        StrView url(url_buffer,200);
        // read a single line
        url_extractor.setString((const char*)buffer);
        
        // put all urls on stack vector
        bool found = url_extractor.nextUrl(url);
        while(found){
          if (!url.isEmpty()){
            if (createEmptyFile(url)) {
              Logger.log(Info, "extractReferences %s",url.c_str());
              stack.push(url.c_str());
            }
          }
          found = url_extractor.nextUrl(url);
        }
    }

};

} // namespace

