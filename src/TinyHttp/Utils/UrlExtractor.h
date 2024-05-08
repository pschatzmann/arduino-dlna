#pragma once

#include "Basic/StrView.h"

namespace tiny_dlna {

/**
 * @brief Extraciting urls from a string the url needs to be stored with delimiting
 * ' or " characters. E.g 'http://www.pschatzmann.ch/test'. This is used to extract
 * all href= entries out of a html page.
 * 
 */

class UrlExtractor {
    public:
        UrlExtractor(const char *root="http://"){
            setRootUrl(root);
        }

        void setRootUrl(const char *prefix="http://"){
            this->prefix = prefix;
            this->prefix_len = strlen(prefix);
        }

        void setString(const char* str){
            int len = strlen(str);
            this->str.set((char*)str,len,len);
            this->start = 0;
        }
    
        void restart(int startPos=0){
            this->start = startPos;            
        }

        bool nextUrl(StrView& result){
            // find all URLs which need to be replaced
            while(true){
                start = str.indexOf(this->prefix, start);
                if (start==-1){
                    result.clear();
                    return false;
                }

                int end = findEndOfString(start-1);
                if (end>=0){
                    // limit end to max characters
                    //int copy_end = end-start>resultMaxLen ? start+resultMaxLen-1 : end;
                    result.substring(str, start, end);
                    // move to next pos
                    start = end+2;
                    return true;
                } else {
                    start+=prefix_len;
                }
            }
            //return false; - this is never reached
        }
    
        const char* c_str() {
            return str.c_str();
        }
        
        const char* getPrefix() {
            return prefix;
        }

    protected:
        const char* prefix;
        int prefix_len;
        StrView str;
        int start;

    
        // finds the closing ' or " character
        int findEndOfString(int pos){
            int end = -1;
            if (pos>=0){
                if (str[pos]=='"'){
                    end = str.indexOf('"',pos+prefix_len);
                } else  if (str[pos]=='\''){
                    end = str.indexOf('\'',pos+prefix_len);
                } else {
                    //DlnaLogger.log(DlnaError, "Invalid string delimiter");
                }
            }
            return end;
        }

};

}

