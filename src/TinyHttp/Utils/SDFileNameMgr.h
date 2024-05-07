#pragma once

#include "Basic/Str.h"
#include "HttpCommon.h"
#include "Utils/MimeResolver.h"
#include <iostream>

namespace tiny_dlna {

/**
 * @brief The file names on an SD can only be 8 characters long. 
 * 
 * In order to support longer names we automatically split up the file name into directories and
 * add a file extension to be able to recover the mime type
 * 
 */
class SDFileNameMgr {
    public:
        SDFileNameMgr(MimeExtension *extensions=nullptr) {
            if (extensions!=nullptr)
                mime_resolver.setValues(extensions);
        }

        // copy constructor
        SDFileNameMgr(SDFileNameMgr &mgr){
            root_url = mgr.root_url;
            name_buffer = mgr.name_buffer;
            mime_resolver = mgr.mime_resolver;
        }

        // defines the root url which is the prefix that will be cut off the file name
        void setRootUrl(const char *root_url){
            this->root_url = root_url;
        }

        // e.g. https://www.pschatzmann.ch/test/longpathname/test [text/html] -> /test/longpath/name/path/test.html
        // builds a compliant file name with an extension triggered by the mime
        virtual StrView& getName(const char *originalName, const char* mime="text/html") {
            name_buffer = originalName;

            if (name_buffer.startsWith(root_url.c_str())){
                // remove leading root by left shift
                name_buffer << root_url.length();
            }

            if (!name_buffer.startsWith("/")){
                name_buffer.insert(0, "/");
            }
            
            // split up the name to be max 8 char long
            split();

            // add extension
            name_buffer += mime_resolver.getExtension(mime);
            return name_buffer;
        }

            
        const char* root() {
            return root_url.c_str();
        }

    protected:
        Str root_url = Str(80);
        Str name_buffer = Str(40);
        MimeResolver mime_resolver;

        void split() {
            int start = 0;
            int end;
            while (true){
                start = name_buffer.indexOf("/", start);
                end = name_buffer.indexOf("/",start+1);
                if (end==-1 || start == -1 || end >= name_buffer.length()){
                    break;
                }    
                // insert delimiting /
                if (end - start > 9){
                    end = start+9;
                    name_buffer.insert(end, "/");
                } 
                start = end;    
            }
        }   
};

}

