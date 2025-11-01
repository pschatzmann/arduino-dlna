#pragma once

namespace tiny_dlna {

/**
 * @brief Mapping from file extensions to mime types
 */

/**
 * @struct MimeExtension
 * @brief Associates a file extension with its corresponding MIME type.
 *
 * Used for mapping file extensions (e.g., ".html") to MIME types (e.g., "text/html")
 * for HTTP responses.
 */
struct MimeExtension {
  /**
   * @brief File extension (including dot), e.g., ".html".
   */
  const char* extension;
  /**
   * @brief Corresponding MIME type string, e.g., "text/html".
   */
  const char* mime;
};


/**
 * @brief Default mapping table of file extensions to MIME types.
 *
 * This table is used by the HTTP server to determine the correct MIME type
 * for files served based on their extension.
 */
static const MimeExtension defaultMimeTable[] = {
  {".htm", "text/html"},       {".css", "text/css"},
  {".xml", "text/xml"},        {".js", "application/javascript"},
  {".png", "image/png"},       {".gif", "image/gif"},
  {".jpeg", "image/jpeg"},     {".ico", "image/x-icon"},
  {".pdf", "application/pdf"}, {".zip", "application/zip"},
  {nullptr, nullptr}};


/**
 * @brief Pointer to the current MIME mapping table.
 *
 * By default, points to defaultMimeTable, but can be redirected to a custom table.
 */
static const MimeExtension* mimeTable = defaultMimeTable;

}  // namespace tiny_dlna
