#pragma once

#include "Basic/Str.h"

namespace tiny_dlna {

/**
 * @brief Object which  information about the rewrite rule
 * 
 */
class HttpRequestRewrite {
    public:
        HttpRequestRewrite(const char *from, const char* to ){
            this->from = StrView(from);
            this->to = StrView(to);
        }
        StrView from;
        StrView to;

};

}
