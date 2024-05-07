#pragma once

#include "HttpCommon.h"

namespace tiny_dlna {

/**
 * @brief Translates between file extensions and mime names
 * 
 */

class MimeResolver {
    public:
        MimeResolver(){
        }

        MimeResolver(MimeExtension *mime_extensions){
            if (mime_extensions!=nullptr)
                setValues(mime_extensions);
        }

        void setValues(MimeExtension *mime_extensions){
            this->mime_extensions = mime_extensions;
        }

        /// determines the mime type from the file extension
        virtual const char* getMime(const char* newName){
            if (mime_extensions!=nullptr){
                Str name(newName);
                for (int j=0; mime_extensions[j].mime != nullptr;j++){
                    if (name.endsWith(mime_extensions[j].extension)){
                        return mime_extensions[j].mime;
                    }
                }
            }
            return nullptr;
        }

        virtual const char* getExtension(const char* mime){
            Str mimeStr(mime);
            if (mime_extensions!=nullptr){
                for (int j=0; mime_extensions[j].mime != nullptr;j++){
                    if (Str(mime_extensions[j].mime) == mimeStr){
                        return mime_extensions[j].extension;
                    }
                }
            }
            return nullptr;
        }


    protected:
        const MimeExtension *mime_extensions = defaultMimeTable;

};

}
