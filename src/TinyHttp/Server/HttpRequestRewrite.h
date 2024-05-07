#pragma once

#include "Basic/StrExt.h"

namespace tiny_dlna {

/**
 * @brief Object which  information about the rewrite rule
 * 
 */
class HttpRequestRewrite {
    public:
        HttpRequestRewrite(const char *from, const char* to ){
            this->from = Str(from);
            this->to = Str(to);
        }
        Str from;
        Str to;

};

}
