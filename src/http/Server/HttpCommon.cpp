
// the functions have been defined in the header so we just need to take care of
// the allocation of the static and shared data

#include "HttpCommon.h"

namespace tiny_dlna {

constexpr const MimeExtension defaultMimeTable[] = {
    {".htm", "text/html"},       {".css", "text/css"},
    {".xml", "text/xml"},        {".js", "application/javascript"},
    {".png", "image/png"},       {".gif", "image/gif"},
    {".jpeg", "image/jpeg"},     {".ico", "image/x-icon"},
    {".pdf", "application/pdf"}, {".zip", "application/zip"},
    {nullptr, nullptr}};

const MimeExtension* mimeTable;

}  // namespace tiny_dlna
