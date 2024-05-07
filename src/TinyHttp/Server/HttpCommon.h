#pragma once

// the functions have been defined in the header so we just need to take care of the
// allocation of the static and shared data

namespace tiny_dlna {

/**
 * @brief Mapping from file extensions to mime types
 * 
 */
struct MimeExtension {
    const char* extension;
    const char* mime;
};

extern const MimeExtension defaultMimeTable[11];
extern const MimeExtension* mimeTable;


} // namespace
