/**
 * @file tinyfatfs-adapter.ino
 * @brief Test for TinyFATFSAdapter with SdFatContentProvider
 *
 * Creates a directory tree on a TinyFATFS RAM disk and checks the recursive
 * listing, the content provider (Browse) and reading files. Prints one line
 * per case and a summary. On the desktop the process exits with 0 if all cases
 * pass and 1 otherwise.
 */
#include <Arduino.h>

#include <string>
#include <vector>

#include "dlna.h"
#include "fatfs.h"
#include "tools/SdFatContentProvider.h"
#include "tools/TinyFATFSAdapter.h"

using namespace tiny_dlna;

RamIO drv{2000, 512};  // 1 MB RAM disk
fatfs::SDClass sd{drv};
TinyFATFSAdapter fs_adapter{sd};
SdFatContentProvider<TinyFATFSAdapter> provider;

int failures = 0;
int count = 0;

void check(bool ok, const char* name) {
  count++;
  if (!ok) failures++;
  Serial.print(ok ? "PASS " : "FAIL ");
  Serial.println(name);
}

/// Print which collects the output in a string
struct StringPrint : public Print {
  size_t write(uint8_t ch) override {
    str += (char)ch;
    return 1;
  }
  std::string str;
};

std::string content(const char* name) {
  return std::string("data of ") + name;
}

bool createFile(const char* path) {
  fatfs::File file = sd.open(path, FILE_WRITE);
  if (!file) return false;
  std::string data = content(path);
  file.write((const uint8_t*)data.data(), data.size());
  file.close();
  return true;
}

bool createTestData() {
  bool ok = true;
  ok &= sd.mkdir("/Music");
  ok &= sd.mkdir("/Music/Artist A");
  ok &= sd.mkdir("/Music/Artist A/Album 1");
  ok &= createFile("/Music/Artist A/Album 1/01 First Song.mp3");
  ok &= createFile("/Music/Artist A/Album 1/02 Second Song.flac");
  ok &= sd.mkdir("/Music/Artist B");
  ok &= createFile("/Music/Artist B/A rather long file name with spaces.wav");
  ok &= sd.mkdir("/Empty");
  ok &= createFile("/readme.txt");
  return ok;
}

/// Browse the children of the indicated object id: returns the titles
std::vector<std::string> browse(const char* objectID) {
  int numberReturned = 0, totalMatches = 0, updateID = 0;
  provider.prepareData(objectID, ContentQueryType::BrowseChildren, "*", 0, 0,
                       "", numberReturned, totalMatches, updateID, nullptr);
  std::vector<std::string> result;
  MediaItem item;
  for (int j = 0; provider.getData(j, item, nullptr); j++) {
    result.push_back(item.title);
  }
  return result;
}

/// Finds the id of the child with the indicated title
int findId(const char* objectID, const char* title) {
  int numberReturned = 0, totalMatches = 0, updateID = 0;
  provider.prepareData(objectID, ContentQueryType::BrowseChildren, "*", 0, 0,
                       "", numberReturned, totalMatches, updateID, nullptr);
  MediaItem item;
  for (int j = 0; provider.getData(j, item, nullptr); j++) {
    if (std::string(item.title) == title) return atoi(item.id);
  }
  return -1;
}

void testListing() {
  StringPrint out;
  check(fs_adapter.chdir("/"), "chdir to the root");
  fs_adapter.ls(&out);
  const char* expected =
      "Music/\r\n"
      "  Artist A/\r\n"
      "    Album 1/\r\n"
      "      01 First Song.mp3\r\n"
      "      02 Second Song.flac\r\n"
      "  Artist B/\r\n"
      "    A rather long file name with spaces.wav\r\n"
      "Empty/\r\n"
      "readme.txt\r\n";
  check(out.str == expected, "recursive listing in SdFat ls() format");
  if (out.str != expected) Serial.println(out.str.c_str());

  StringPrint sub;
  check(fs_adapter.chdir("/Music/Artist A"), "chdir to a subdirectory");
  fs_adapter.ls(&sub);
  check(sub.str ==
            "Album 1/\r\n  01 First Song.mp3\r\n  02 Second Song.flac\r\n",
        "listing of a subdirectory");
  check(!fs_adapter.chdir("/missing"), "chdir to a missing directory fails");
  check(!fs_adapter.chdir("/readme.txt"), "chdir to a file fails");
}

void testContentProvider() {
  check(provider.begin(fs_adapter, "/"), "content provider begin");
  // root + 9 entries
  check(provider.size() == 10, "tree contains all 10 nodes");

  std::vector<std::string> root = browse("0");
  check(root == std::vector<std::string>({"Music", "Empty", "readme.txt"}),
        "browse root");

  int music = findId("0", "Music");
  int artist = findId(std::to_string(music).c_str(), "Artist A");
  int album = findId(std::to_string(artist).c_str(), "Album 1");
  std::vector<std::string> songs = browse(std::to_string(album).c_str());
  check(songs == std::vector<std::string>(
                     {"01 First Song.mp3", "02 Second Song.flac"}),
        "browse album");
  check(browse(std::to_string(findId("0", "Empty")).c_str()).empty(),
        "browse empty directory");

  int song = findId(std::to_string(album).c_str(), "01 First Song.mp3");
  TreeNode& node = provider.getTreeNodeById(song);
  check(node.path() == "/Music/Artist A/Album 1/01 First Song.mp3",
        "path of a file");
  check(node.getMime() == "audio/mpeg", "mime of a file");

  // read and seek the file via the adapter, as the file handler does
  fatfs::File file = fs_adapter.open(node.path().c_str());
  std::string expected = content(node.path().c_str());
  check((bool)file && file.size() == expected.size(), "open file: size");
  char buffer[100] = {0};
  file.seek(8);
  size_t len = file.readBytes((uint8_t*)buffer, sizeof(buffer));
  check(std::string(buffer, len) == expected.substr(8),
        "read file after seek");
  file.close();
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Warning);
  if (!sd.begin()) {
    Serial.println("FAIL sd.begin()");
    exit(1);
  }
  check(createTestData(), "create test data on the RAM disk");
  testListing();
  testContentProvider();

  Serial.print(count - failures);
  Serial.print(" of ");
  Serial.print(count);
  Serial.println(failures == 0 ? " tests passed" : " tests passed: FAILED");
#ifdef IS_DESKTOP
  exit(failures == 0 ? 0 : 1);
#endif
}

void loop() { delay(1000); }
