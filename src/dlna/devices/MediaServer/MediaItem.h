#pragma once


namespace tiny_dlna {

// Music: <upnp:class>object.item.audioItem.musicTrack</upnp:class>
// Radio: <upnp:class>object.item.audioItem.audioBroadcast</upnp:class>
// Video: <upnp:class>object.item.videoItem.movie</upnp:class>
// Photo: <upnp:class>object.item.imageItem.photo</upnp:class>
// Folder: <upnp:class>object.container.storageFolder</upnp:class>

enum class MediaItemClass {
  Unknown,
  Folder,
  Music,
  Radio,
  Video,
  Photo
};

/// @brief Media item description used to build DIDL-Lite entries
struct MediaItem {
  const char* id = nullptr;
  const char* parentID = "0";
  bool restricted = true;
  const char* title = nullptr;
  const char* resourceURI = nullptr;  // resource URL
  const char* mimeType = nullptr;
  MediaItemClass itemClass = MediaItemClass::Unknown;
  const char* albumArtURI = nullptr;  // optional album art URI
  // Additional optional metadata fields could be added here (duration,
  // creator...)
};

}  // namespace tiny_dlna
