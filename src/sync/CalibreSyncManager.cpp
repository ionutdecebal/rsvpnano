#include "sync/CalibreSyncManager.h"

#include <SD_MMC.h>

#include <utility>

#include "net/WifiConnection.h"
#include "storage/StorageManager.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "text/AsciiText.h"

// The reconciling sync engine. Networking is delegated to net::HttpFetch and
// CalibreClient; the reconcile diff is the pure calibresync::computeSyncPlan()
// in src/sync/CalibreSyncPlan.h. JSON is hand-rolled with the
// indexOf/substring idiom shared with CompanionSyncManager and ReleaseParser
// (this repo does NOT use ArduinoJson). Logging uses the same
// Serial.printf("[tag] ...") style; the tag here is "[calibre-sync]".

namespace {

constexpr const char *kLogTag = "[calibre-sync]";

// Cap a single book download. calibre .rsvp files are small text; 8 MiB is a
// generous ceiling that still protects RAM/SD from a runaway response.
constexpr size_t kMaxBookBytes = 8UL * 1024UL * 1024UL;

bool isSafeFilenameChar(char c) {
  return AsciiText::isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' ||
         c == ' ';
}

bool ensureLibraryDirectories() {
  return StorageFiles::ensureDirectory(StoragePaths::kBooksPath,
                                       "calibre-sync") &&
         StorageFiles::ensureDirectory(StoragePaths::kBookFilesPath,
                                       "calibre-sync") &&
         StorageFiles::ensureDirectory(StoragePaths::kArticleFilesPath,
                                       "calibre-sync");
}

String jsonEscape(const String &value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

// ---- Minimal hand-rolled JSON scanners, mirroring CompanionSyncManager -----

int skipWs(const String &body, int index) {
  while (index < static_cast<int>(body.length()) &&
         AsciiText::isWhitespace(body[index])) {
    ++index;
  }
  return index;
}

// Reads the JSON string literal whose opening quote is at quoteIndex, decoding
// the handful of escapes Calibre/manifest writers emit. Returns the index just
// past the closing quote, or -1 on malformed input.
int readJsonStringAt(const String &body, int quoteIndex, String &value) {
  if (quoteIndex < 0 || quoteIndex >= static_cast<int>(body.length()) ||
      body[quoteIndex] != '"') {
    return -1;
  }
  String result;
  int index = quoteIndex + 1;
  while (index < static_cast<int>(body.length())) {
    const char c = body[index++];
    if (c == '"') {
      value = result;
      return index;
    }
    if (c == '\\' && index < static_cast<int>(body.length())) {
      const char escaped = body[index++];
      switch (escaped) {
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        default:  // ", \, /, and anything else: keep the literal char
          result += escaped;
          break;
      }
    } else {
      result += c;
    }
  }
  return -1;
}

}  // namespace

const char *CalibreSyncManager::manifestPath() {
  return "/books/.calibre-sync.json";
}

String CalibreSyncManager::sanitizeBaseName(const String &name) {
  String sanitized;
  sanitized.reserve(name.length());
  for (size_t i = 0; i < name.length(); ++i) {
    const char c = name[i];
    sanitized += isSafeFilenameChar(c) ? c : '-';
  }
  sanitized.trim();
  while (sanitized.startsWith(".")) {
    sanitized.remove(0, 1);
  }
  // Cap the base name length the way the companion uploader does (72 chars).
  if (sanitized.length() > 72) {
    sanitized = sanitized.substring(0, 72);
    sanitized.trim();
  }
  return sanitized;
}

const char *CalibreSyncManager::targetDirectoryFor(
    const calibresync::RemoteEntry &remote) {
  (void)remote;
  // Default folder routing: all books land in /books/books.
  //
  // TODO(future tag rule): when a Calibre tag (e.g. "article") is available on
  // the resolved book, route those to StoragePaths::kArticleFilesPath
  // (/books/articles) instead. The tag is not currently surfaced by
  // CalibreClient::resolveRsvp(), so the rule is left unimplemented -- this
  // function is the single hook where it will plug in.
  return StoragePaths::kBookFilesPath;
}

String CalibreSyncManager::destinationPath(
    const calibresync::RemoteEntry &remote) {
  String base = sanitizeBaseName(remote.title);
  if (base.isEmpty()) {
    base = String(remote.id);
  }
  return String(targetDirectoryFor(remote)) + "/" + base + ".rsvp";
}

String CalibreSyncManager::changeKey(const CalibreClient::RsvpRef &ref) {
  // Raw-string change-key: size + "|" + mtime. Compared byte-for-byte; never
  // parsed into an epoch (the device has no reliable clock).
  return String(static_cast<uint32_t>(ref.size)) + "|" + ref.lastModified;
}

bool CalibreSyncManager::parseManifest(
    const String &json, std::vector<calibresync::ManifestEntry> &out) {
  out.clear();

  // Shape: {"version":1,"books":{"<id>":{"key":"..","path":"..","title":".."}}}
  const int booksKey = json.indexOf("\"books\"");
  if (booksKey < 0) {
    return false;
  }
  int index = json.indexOf('{', booksKey);
  if (index < 0) {
    return false;
  }
  ++index;  // step past the opening brace of the books object

  while (index < static_cast<int>(json.length())) {
    index = skipWs(json, index);
    if (index >= static_cast<int>(json.length()) || json[index] == '}') {
      break;  // end of books object
    }
    if (json[index] == ',') {
      ++index;
      continue;
    }
    if (json[index] != '"') {
      ++index;  // be forgiving about unexpected punctuation
      continue;
    }

    // The key is the book id as a JSON string.
    String idStr;
    const int afterId = readJsonStringAt(json, index, idStr);
    if (afterId < 0) {
      break;
    }
    index = skipWs(json, afterId);
    if (index >= static_cast<int>(json.length()) || json[index] != ':') {
      break;
    }
    index = skipWs(json, index + 1);
    if (index >= static_cast<int>(json.length()) || json[index] != '{') {
      break;
    }

    // Find the matching close brace for this entry object.
    const int entryStart = index;
    int depth = 0;
    int scan = index;
    bool inString = false;
    while (scan < static_cast<int>(json.length())) {
      const char c = json[scan];
      if (inString) {
        if (c == '\\') {
          ++scan;  // skip the escaped char
        } else if (c == '"') {
          inString = false;
        }
      } else if (c == '"') {
        inString = true;
      } else if (c == '{') {
        ++depth;
      } else if (c == '}') {
        --depth;
        if (depth == 0) {
          ++scan;
          break;
        }
      }
      ++scan;
    }
    const String entry = json.substring(entryStart, scan);

    calibresync::ManifestEntry parsed;
    parsed.id = idStr.toInt();

    const int keyIdx = entry.indexOf("\"key\"");
    if (keyIdx >= 0) {
      const int q = entry.indexOf('"', entry.indexOf(':', keyIdx) + 1);
      readJsonStringAt(entry, q, parsed.key);
    }
    const int pathIdx = entry.indexOf("\"path\"");
    if (pathIdx >= 0) {
      const int q = entry.indexOf('"', entry.indexOf(':', pathIdx) + 1);
      readJsonStringAt(entry, q, parsed.path);
    }

    out.push_back(parsed);
    index = scan;
  }

  return true;
}

String CalibreSyncManager::serializeManifest(
    const std::vector<calibresync::ManifestEntry> &entries,
    const std::vector<String> &titles) {
  String body;
  body.reserve(128 + entries.size() * 96);
  body += "{\"version\":1,\"books\":{";
  bool first = true;
  for (size_t i = 0; i < entries.size(); ++i) {
    const calibresync::ManifestEntry &e = entries[i];
    const String title = i < titles.size() ? titles[i] : String();
    if (!first) {
      body += ",";
    }
    first = false;
    body += "\"" + String(e.id) + "\":{";
    body += "\"key\":\"" + jsonEscape(e.key) + "\",";
    body += "\"path\":\"" + jsonEscape(e.path) + "\",";
    body += "\"title\":\"" + jsonEscape(title) + "\"";
    body += "}";
  }
  body += "}}";
  return body;
}

bool CalibreSyncManager::downloadTo(const String &url, const String &path,
                                    const net::HttpAuth &auth) {
  const String tmpPath = path + ".tmp";
  SD_MMC.remove(tmpPath);
  File out = SD_MMC.open(tmpPath, FILE_WRITE);
  if (!out) {
    Serial.printf("%s could not open %s for write\n", kLogTag, tmpPath.c_str());
    return false;
  }

  bool writeOk = true;
  const net::HttpSink sink = [&out, &writeOk](const uint8_t *data,
                                              size_t length) -> bool {
    if (!writeOk) {
      return false;
    }
    if (out.write(data, length) != length) {
      writeOk = false;
      return false;
    }
    return true;
  };

  const net::HttpResult res = net::get(url, sink, auth, kMaxBookBytes);
  out.close();

  if (!res.ok || !writeOk) {
    Serial.printf("%s download failed url=%s status=%d err=%s\n", kLogTag,
                  url.c_str(), res.statusCode, res.error.c_str());
    SD_MMC.remove(tmpPath);
    return false;
  }

  SD_MMC.remove(path);
  if (!SD_MMC.rename(tmpPath, path)) {
    Serial.printf("%s rename %s -> %s failed\n", kLogTag, tmpPath.c_str(),
                  path.c_str());
    SD_MMC.remove(tmpPath);
    return false;
  }
  return true;
}

void CalibreSyncManager::report(const String &phase, int current, int total,
                                const String &detail) {
  if (progress_) {
    progress_(phase, current, total, detail);
  }
}

CalibreSyncManager::Result CalibreSyncManager::runSync(
    const CalibreSettings &settings, const String &wifiSsid,
    const String &wifiPassword) {
  Result result;

  if (!settings.enabled) {
    result.ok = true;
    return result;
  }

  report("wifi", 0, 0, wifiSsid);
  if (!net::connectStation(wifiSsid, wifiPassword)) {
    result.error = "WiFi connect failed";
    report("error", 0, 0, result.error);
    return result;
  }

  result = reconcile(settings);

  net::disconnect();
  return result;
}

CalibreSyncManager::Result CalibreSyncManager::reconcile(
    const CalibreSettings &settings) {
  Result result;

  if (!settings.enabled) {
    result.ok = true;
    return result;
  }

  if (!ensureLibraryDirectories()) {
    result.error = "Library folders unavailable";
    report("error", 0, 0, result.error);
    return result;
  }

  net::HttpAuth auth;
  auth.username = settings.username;
  auth.password = settings.password;

  CalibreClient::Config clientConfig;
  clientConfig.baseUrl = settings.baseUrl;
  clientConfig.libraryId = settings.libraryId;
  clientConfig.auth = auth;
  CalibreClient client(clientConfig);

  // 1. search -> book ids
  report("search", 0, 0, settings.searchQuery);
  const std::vector<int> ids = client.search(settings.searchQuery);
  Serial.printf("%s search returned %u ids\n", kLogTag,
                static_cast<unsigned>(ids.size()));

  // 2. resolve each id to a RemoteEntry (skip books with no rsvp format).
  std::vector<calibresync::RemoteEntry> remote;
  remote.reserve(ids.size());
  int resolved = 0;
  for (const int id : ids) {
    ++resolved;
    report("resolve", resolved, static_cast<int>(ids.size()), String(id));
    CalibreClient::RsvpRef ref;
    if (!client.resolveRsvp(id, ref)) {
      continue;  // no downloadable rsvp format -- not an error
    }
    calibresync::RemoteEntry entry;
    entry.id = id;
    entry.key = changeKey(ref);
    entry.url = client.downloadUrl(ref);
    // CalibreClient::resolveRsvp() does not surface a human title (the
    // /ajax/book payload carries one, but RsvpRef intentionally exposes only
    // the download ref + change-key). We therefore name files by id, which is
    // stable and clock-free. If a future CalibreClient revision adds a title to
    // RsvpRef, set entry.title here and destinationPath() will prefer it.
    entry.title = String();
    if (entry.url.isEmpty()) {
      continue;
    }
    remote.push_back(entry);
  }

  // 3. read the on-SD manifest.
  std::vector<calibresync::ManifestEntry> manifest;
  {
    File mf = SD_MMC.open(manifestPath());
    if (mf && !mf.isDirectory()) {
      String body;
      body.reserve(static_cast<size_t>(mf.size()) + 1);
      while (mf.available()) {
        body += static_cast<char>(mf.read());
      }
      parseManifest(body, manifest);
    }
    if (mf) {
      mf.close();
    }
  }

  // 4. diff (pure core).
  const calibresync::DeletionPolicy policy =
      settings.deletionPolicy == CalibreSettings::Mirror
          ? calibresync::DeletionPolicy::Mirror
          : calibresync::DeletionPolicy::Keep;
  const calibresync::SyncPlan plan =
      calibresync::computeSyncPlan(remote, manifest, policy);
  result.unchanged = static_cast<int>(plan.unchanged.size());
  Serial.printf("%s plan: %u download, %u delete, %u unchanged\n", kLogTag,
                static_cast<unsigned>(plan.toDownload.size()),
                static_cast<unsigned>(plan.toDelete.size()),
                static_cast<unsigned>(plan.unchanged.size()));

  // Build the next manifest starting from entries we keep (unchanged + any
  // manifest entry that survives reconcile). We key by id so updates replace
  // cleanly.
  std::vector<calibresync::ManifestEntry> nextEntries;
  std::vector<String> nextTitles;

  // Index remote entries by id for path/title/key lookups during writes.
  const auto remoteById = [&remote](int id) -> const calibresync::RemoteEntry * {
    for (const calibresync::RemoteEntry &r : remote) {
      if (r.id == id) {
        return &r;
      }
    }
    return nullptr;
  };

  // Seed with surviving manifest entries (unchanged, and -- under Keep --
  // entries that left search scope but are not deleted).
  std::vector<int> deletedIds;
  deletedIds.reserve(plan.toDelete.size());
  for (const calibresync::DeleteAction &d : plan.toDelete) {
    deletedIds.push_back(d.id);
  }
  const auto isPlannedDelete = [&deletedIds](int id) {
    for (const int d : deletedIds) {
      if (d == id) {
        return true;
      }
    }
    return false;
  };
  const auto isPlannedDownload = [&plan](int id) {
    for (const calibresync::DownloadAction &a : plan.toDownload) {
      if (a.id == id) {
        return true;
      }
    }
    return false;
  };

  for (const calibresync::ManifestEntry &m : manifest) {
    if (isPlannedDelete(m.id) || isPlannedDownload(m.id)) {
      continue;  // handled below (re-download replaces, delete removes)
    }
    nextEntries.push_back(m);
    // Title is not needed by reconcile; preserve "" rather than re-parse.
    nextTitles.push_back(String());
  }

  // 5. downloads.
  int downloadIndex = 0;
  for (const calibresync::DownloadAction &action : plan.toDownload) {
    ++downloadIndex;
    const calibresync::RemoteEntry *r = remoteById(action.id);
    if (r == nullptr) {
      continue;
    }
    const String path = destinationPath(*r);
    report("download", downloadIndex, static_cast<int>(plan.toDownload.size()),
           r->title);
    if (downloadTo(action.url, path, auth)) {
      calibresync::ManifestEntry entry;
      entry.id = action.id;
      entry.key = action.key;
      entry.path = path;
      nextEntries.push_back(entry);
      nextTitles.push_back(r->title);
      ++result.downloaded;
      Serial.printf("%s downloaded id=%d -> %s\n", kLogTag, action.id,
                    path.c_str());
    } else {
      ++result.failed;
    }
  }

  // 6. deletions (Mirror only -- plan.toDelete is empty under Keep).
  int deleteIndex = 0;
  for (const calibresync::DeleteAction &action : plan.toDelete) {
    ++deleteIndex;
    report("delete", deleteIndex, static_cast<int>(plan.toDelete.size()),
           action.path);
    if (!action.path.isEmpty()) {
      SD_MMC.remove(action.path);
    }
    ++result.deleted;
    Serial.printf("%s deleted id=%d -> %s\n", kLogTag, action.id,
                  action.path.c_str());
  }

  // 7. rewrite the manifest.
  {
    const String body = serializeManifest(nextEntries, nextTitles);
    const String tmpPath = String(manifestPath()) + ".tmp";
    SD_MMC.remove(tmpPath);
    File mf = SD_MMC.open(tmpPath, FILE_WRITE);
    if (mf) {
      mf.print(body);
      mf.close();
      SD_MMC.remove(manifestPath());
      SD_MMC.rename(tmpPath, manifestPath());
    } else {
      Serial.printf("%s could not write manifest\n", kLogTag);
    }
  }

  // 8. trigger a library reindex so the device picks up the changes.
  if (storage_ != nullptr) {
    storage_->refreshBooks();
  }

  result.ok = true;
  report("done", result.downloaded, result.downloaded, String());
  Serial.printf("%s done: %d downloaded, %d deleted, %d unchanged, %d failed\n",
                kLogTag, result.downloaded, result.deleted, result.unchanged,
                result.failed);
  return result;
}
